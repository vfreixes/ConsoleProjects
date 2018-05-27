#pragma once

#include "TaskManager.hpp"

namespace Utilities
{
	namespace TaskManager
	{
		// funcions del JobContext per facilitar-ne l'ús
		inline void JobContext::Do(Job* job) const { scheduler->Do(job, this); }
		inline void JobContext::Wait(Job* job) const { scheduler->Wait(job, this); }
		inline void JobContext::DoAndWait(Job* job) const { scheduler->DoAndWait(job, this); }


		// bucle de cada fiber
		inline void __stdcall WorkerFiber(void* param)
		{
			JobScheduler::FiberContext &fiberContext = *reinterpret_cast<JobScheduler::FiberContext*>(param); // adquirim el context

			while (fiberContext.scheduler->runTasks)
			{

				fiberContext.job->DoTask(fiberContext.taskIndex, fiberContext); // executem la tasca que tenim assignada
				fiberContext.job->TaskFinished(); // marquem la tasca com a finalitzada

				fiberContext.scheduler->SwitchToFiber(fiberContext.scheduler->rootFibers[fiberContext.threadIndex]); // tornem al nostre scheduler
			}
		}


		// funció principal que assigna la feina per cada thread
		inline void JobScheduler::RunScheduler(int idx, Profiler &profiler)
		{
			short fibersOnWait[NumFibers]; // stack de fibers que estan esperant a altres per a continuar
			int numFibersOnWait = 0; // 

			// ens guardem quan va acabar l'última tasca que vam poder executar per a dormir si no en trobem cap més en prou temps
			std::chrono::high_resolution_clock::time_point lastJobCompletedTime = std::chrono::high_resolution_clock::now();

			while (runTasks)
			{
				// primer comprovem si alguna de les tasques que estan esperant a altres pot continuar
				for (int i = 0; i < numFibersOnWait; ++i)
				{
					short fiberIndex = fibersOnWait[i];
					FiberContext &fiberContext = fiberContexts[fiberIndex];
					// if the fiber is waiting for itself it means that blocked when adding jobs (the queue was full), so we wake it up
					// if is waiting for another job, we check if it is finished
					if (fiberContext.fiberWaitingForJobCompletion == fiberContext.job || fiberContext.fiberWaitingForJobCompletion->HasFinished())
					{
						fiberContext.fiberWaitingForJobCompletion = nullptr; // marquem que la tasca no espera a ningú

						// marquem al profiler que la tasca continua la seva feina
						profiler.AddProfileMark(Profiler::MarkerType::RESUME_FROM_PAUSE, (void*)(intptr_t)fiberIndex, fiberContext.job->jobName, idx, fiberContext.job->systemID);

						// entrem a la Fiber en qüestió
						SwitchToFiber(fibers[fiberIndex]);
						lastJobCompletedTime = std::chrono::high_resolution_clock::now(); // marquem que hem acabat una tasca

						// mirem si la tasca ha completat o està esperant alguna cosa
						if (fiberContext.fiberWaitingForJobCompletion == nullptr)
						{
							profiler.AddProfileMark(Profiler::MarkerType::END, (void*)(intptr_t)fiberIndex, fiberContext.job->jobName, idx, fiberContext.job->systemID);

							// wake up any thread that may be idle, as a prerequisite may be finished
							NotifyWaitingThreads();

							fiberContext.job = nullptr; // Not strictly necessary, but still...
							fiberContext.taskIndex = -1;

							// tornem la fiber a la llista de disponibles
							if (fiberIndex < NumSmallStackFibers)
								smallStackFiberIndexs.Push(fiberIndex);
							else
								largeStackFiberIndexs.Push(fiberIndex);

							// remove this from queue
							--numFibersOnWait;
							fibersOnWait[i] = fibersOnWait[numFibersOnWait];
							--i;
						}
						else
						{
							// la tasca continua esperant alguna cosa, ho marquem i no toquem res més.
							profiler.AddProfileMark(
								(fiberContext.fiberWaitingForJobCompletion == fiberContext.job)
								? Profiler::MarkerType::PAUSE_WAIT_FOR_QUEUE_SPACE
								: Profiler::MarkerType::PAUSE_WAIT_FOR_JOB,
								(void*)(intptr_t)fiberIndex, fiberContext.job->jobName, idx, fiberContext.job->systemID);
						}
					}
				}

				// iterem per les cues, començant per la més prioritaria, buscant alguna tasca per a completar
				for (int i = 0; i < 3; ++i)
				{
					Job* job;
					int taskIndex;
					if (queues[i].GetPendingTask(job, taskIndex)) // si la cua actual té alguna cosa...
					{
						// agafem la tasca del stack corresponent
						short fiberIndex = job->needsLargeStack ? largeStackFiberIndexs.Pop() : smallStackFiberIndexs.Pop();
						FiberContext &fiberContext = fiberContexts[fiberIndex];

						// completem la info del contexte per que el Fiber sàpiga què ha de fer
						fiberContext.job = job;
						fiberContext.taskIndex = taskIndex;
						fiberContext.threadIndex = idx;
						fiberContext.fiberWaitingForJobCompletion = nullptr;

						profiler.AddProfileMark(Profiler::MarkerType::BEGIN, (void*)(intptr_t)fiberIndex, fiberContext.job->jobName, idx, fiberContext.job->systemID);

						// entrem a la Fiber en qüestió
						SwitchToFiber(fibers[fiberIndex]);
						lastJobCompletedTime = std::chrono::high_resolution_clock::now(); // marquem que hem acabat una tasca

						// mirem si la tasca ha completat o està esperant alguna cosa
						if (fiberContext.fiberWaitingForJobCompletion == nullptr)
						{
							profiler.AddProfileMark(Profiler::MarkerType::END, (void*)(intptr_t)fiberIndex, fiberContext.job->jobName, idx, fiberContext.job->systemID);

							// wake up any thread that may be idle, as a prerequisite may be finished
							NotifyWaitingThreads();

							fiberContext.job = nullptr; // Not strictly necessary, but still...
							fiberContext.taskIndex = -1;

							if (fiberIndex < NumSmallStackFibers)
								smallStackFiberIndexs.Push(fiberIndex);
							else
								largeStackFiberIndexs.Push(fiberIndex);
						}
						else
						{
							profiler.AddProfileMark(
								(fiberContext.fiberWaitingForJobCompletion == fiberContext.job)
								? Profiler::MarkerType::PAUSE_WAIT_FOR_QUEUE_SPACE
								: Profiler::MarkerType::PAUSE_WAIT_FOR_JOB,
								(void*)(intptr_t)fiberIndex, fiberContext.job->jobName, idx, fiberContext.job->systemID);

							// ens guardem aquesta fiber com a que està esperant quelcom
							fibersOnWait[numFibersOnWait] = fiberIndex;
							++numFibersOnWait;
						}

						break;
					}
				}

				// mirem si el thread porta 500 micro segons sense res per fer. En aqeust cas l'adormim.
				std::chrono::microseconds usSinceLastCompletedJob = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - lastJobCompletedTime);
				using namespace std::chrono_literals;
				if (usSinceLastCompletedJob > 500us)
				{
					profiler.AddProfileMark(Profiler::MarkerType::BEGIN_IDLE, nullptr, "Idle", idx);
					WaitForNotification(idx);
					profiler.AddProfileMark(Profiler::MarkerType::END_IDLE, nullptr, nullptr, idx);

					lastJobCompletedTime = std::chrono::high_resolution_clock::now();
				}
			}
		}
	}
}
