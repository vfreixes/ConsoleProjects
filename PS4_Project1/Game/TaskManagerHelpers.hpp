#pragma once

#include "TaskManager.hpp"

namespace Utilities
{
	namespace TaskManager
	{
		// classes que ens permetràn crear tasques amb lambdes de C++11
		// mirar el final del fitxer
		
		
		template<typename Lambda, bool has_context_call>
		struct LambdaCaller
		{
			static_assert(std::is_convertible < Lambda, std::function<void(int, const JobContext&)>>::value, "Lambda must have the form 'void(int)' or 'void(int, const JobContext&)'");

			LambdaCaller(Lambda& lambda, int taskIndex, const JobContext& context)
			{
				lambda(taskIndex, context);
			}
		};

		template<typename Lambda>
		struct LambdaCaller<Lambda, false>
		{
			static_assert(std::is_convertible < Lambda, std::function<void(int)>>::value, "Lambda must have the form 'void(int)' or 'void(int, const JobContext&)'");

			LambdaCaller(Lambda& lambda, int taskIndex, const JobContext& context)
			{
				lambda(taskIndex);
			}
		};


		template<typename Lambda>
		class LambdaJob : public Job
		{
			Lambda lambda;

		public:

			LambdaJob(const Lambda& _lambda, const char* _jobName, short _numTasks = 1, int _systemID = -1, Job::Priority _priority = Job::Priority::MEDIUM, bool _needsLargeStack = false)
				: Job(_jobName, _numTasks, _systemID, _priority, _needsLargeStack)
				, lambda(_lambda)
			{

			}

			void DoTask(int taskIndex, const JobContext& context) override
			{
				LambdaCaller<Lambda, std::is_convertible<Lambda, std::function<void(int, const JobContext&)>>::value >(lambda, taskIndex, context);
			}
		};

		template<typename Lambda>
		class LambdaBatchedJob : public Job
		{
			Lambda lambda;
			const int batchSize;
			const int totalTasks;

		public:

			LambdaBatchedJob(const Lambda& _lambda, const char* _jobName, int _batchSize, short _numTasks, int _systemID = -1, Job::Priority _priority = Job::Priority::MEDIUM, bool _needsLargeStack = false)
				: Job(_jobName, ((_numTasks - 1) / _batchSize) + 1, _systemID, _priority, _needsLargeStack)
				, lambda(_lambda)
				, batchSize(_batchSize)
				, totalTasks(_numTasks)
			{

			}

			void DoTask(int taskIndex, const JobContext& context) override
			{
				int max = totalTasks < (taskIndex + 1) * batchSize ? totalTasks : (taskIndex + 1) * batchSize;
				for (int i = taskIndex * batchSize; i < max; ++i)
					LambdaCaller<Lambda, std::is_convertible<Lambda, std::function<void(int, const JobContext&)>>::value>(lambda, i, context);
			}
		};

		// HERE

		// crea una tasca a partir d'una lambda.
		template<typename Lambda>
		LambdaJob<Lambda> CreateLambdaJob(const Lambda& _lambda, const char* _jobName, short _numTasks = 1, int _systemID = -1, Job::Priority _priority = Job::Priority::MEDIUM, bool _needsLargeStack = false)
		{
			return LambdaJob<Lambda>(_lambda, _jobName, _numTasks, _systemID, _priority, _needsLargeStack);
		}

		// crea una tasca "batched". Que serà cridada per grups dins dels fibers
		template<typename Lambda>
		LambdaBatchedJob<Lambda> CreateLambdaBatchedJob(const Lambda& _lambda, const char* _jobName, short _batchSize, short _numTasks, int _systemID = -1, Job::Priority _priority = Job::Priority::MEDIUM, bool _needsLargeStack = false)
		{
			return LambdaBatchedJob<Lambda>(_lambda, _jobName, _batchSize, _numTasks, _systemID, _priority, _needsLargeStack);
		}
	}
}

//{
//	auto job = CreateLambdaJob(
//		[](int taskIndex, const JobContext& context)
//		{
//			printf("%d\n", taskIndex);
//		},
//		"printer",
//		100// nº de vegades que es farà la tasca.
//	);
//	
//	auto job2 = CreateLambdaBatchedJob(
//		[](int taskIndex, const JobContext& context)
//		{
//			printf("%d\n", taskIndex);
//		},
//		"batched printer",
//		20, // grups de 20 execucions
//		100 // nº TOTAL de vegades que es farà la tasca.
//	);
//	
//	
//	context.Do(&job);
//	context.DoAndWait(&job2);
//	context.Wait(&job);
//}