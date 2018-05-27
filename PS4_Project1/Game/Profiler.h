#include <chrono>
#include <atomic>
#include <string>

#include "imgui.h"

namespace Utilities
{
	class Profiler
	{
	public:
		enum class MarkerType : unsigned char {
			BEGIN,
			END,
			PAUSE_WAIT_FOR_JOB,
			PAUSE_WAIT_FOR_QUEUE_SPACE,
			RESUME_FROM_PAUSE,
			BEGIN_IDLE, END_IDLE,
			BEGIN_FUNCTION, END_FUNCTION,
		};

		// Cridar aquesta funció cada cop que volguem registrar una marca al profiler.
		// emparellar cada BEGIN_* amb un END_* i cada PAUSE_* amb un RESUME_*. Els BEGIN_FUNCTION/END_FUNCTION han d'anar aniuats i dins de begin/end
		void AddProfileMark(MarkerType reason, const void* identifier = nullptr, const char* functionName = nullptr, int threadId = 0, int systemID = -1)
		{
			if (recordNewFrame)
			{
				ProfileMarker mark{ std::chrono::high_resolution_clock::now(), identifier, functionName, systemID, reason };

				profilerData[threadId][profilerNextWriteIndex[threadId]] = mark;
				profilerNextWriteIndex[threadId] = (profilerNextWriteIndex[threadId] + 1) % ProfilerMarkerBufferSize;
			}
		}

		// crea un "BEGIN_FUNCTION" i quan l'objecte creat es destrueix, un "END_FUNCTION"
		struct MarkGuard;
		MarkGuard CreateProfileMarkGuard(const char* functionName, int threadId = 0, int systemID = -1)
		{
			static std::atomic<uintptr_t> identifierSequence;
			void* id = reinterpret_cast<void*>(++identifierSequence);

			AddProfileMark(MarkerType::BEGIN_FUNCTION, id, functionName, threadId, systemID);
			return MarkGuard(this, threadId, id);
		}

		// dibuixa una finestra ImGUI amb la info del darrer frame
		void DrawProfilerToImGUI(int numThreads)
		{
			if (ImGui::Begin("Profiler"))
			{
				ImGui::Checkbox("Record", &recordNewFrame);
				ImGui::SliderFloat("Scale", &millisecondLength, 20, 100000, "%.3f", 5);

				ImGui::BeginChild("scrolling", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

				ImDrawList* draw_list = ImGui::GetWindowDrawList();

				// busquem els temps mínims i màxims del frame per a colocar les coses al seu voltant	
				std::chrono::high_resolution_clock::time_point earliestTimePoint;
				std::chrono::high_resolution_clock::time_point latestTimePoint;
				bool earliestInitialized = false;
				bool latestInitialized = false;
				for (int l = 0; l < numThreads; l++)
				{

					if (profilerNextReadIndex[l] != profilerNextWriteIndex[l])
					{
						int index = (profilerNextWriteIndex[l] - 1 + ProfilerMarkerBufferSize) % ProfilerMarkerBufferSize;
						const ProfileMarker &marker = profilerData[l][index];

						if (latestTimePoint < marker.timePoint || !latestInitialized)
						{
							latestTimePoint = marker.timePoint;
							latestInitialized = true;
						}
					}
					for (int i = profilerNextReadIndex[l]; i != profilerNextWriteIndex[l]; i = (i + 1) % ProfilerMarkerBufferSize)
					{
						const ProfileMarker &marker = profilerData[l][i];
						if (marker.IsBeginMark() && !marker.IsIdleMark())
						{
							if (earliestTimePoint > marker.timePoint || !earliestInitialized)
							{
								earliestTimePoint = marker.timePoint;
								earliestInitialized = true;
							}
							break;
						}
					}
				}

				// limitem les coses a 1s per no forçar massa l'ImGUI
				auto maxMs = std::chrono::duration_cast<std::chrono::milliseconds>(latestTimePoint - earliestTimePoint).count() + 2;

				if (maxMs > 1000)
				{
					maxMs = 1000;
				}

				// pintem llegendes orientatives.
				ImVec2 cursor = ImGui::GetCursorScreenPos();
				float offset = 50, lineOffset = cursor.x;
				for (int n = 0; n < maxMs; n++)
				{
					if (n > 0) ImGui::SameLine();
					cursor.x = offset + n * millisecondLength;
					float hue = (n < 16) ? 0.33f - n * 0.08f / 15.0f : (n < 33) ? 0.16f - (n - 16) * 0.08f / 15.0f : 0;
					draw_list->AddLine(ImVec2(cursor.x + lineOffset, cursor.y), ImVec2(cursor.x + lineOffset, cursor.y + 400), ImColor::HSV(hue, 1, 1), 1.0f);
					ImGui::SetCursorPosX(cursor.x);
					ImGui::LabelText("", "%dms", n);
				}

				{
					auto fmaxMs = std::chrono::duration_cast<std::chrono::duration<float, std::milli>>(latestTimePoint - earliestTimePoint).count();
					float hue = (fmaxMs < 16) ? 0.33f - fmaxMs * 0.08f / 15.0f : (fmaxMs < 33) ? 0.16f - (fmaxMs - 16) * 0.08f / 15.0f : 0;

					cursor.x = offset + fmaxMs * millisecondLength + lineOffset;
					draw_list->AddLine(cursor, ImVec2(cursor.x, cursor.y + 400), ImColor::HSV(hue, 1, 1), 1.0f);
				}

				// funció per a pintar 1 periode
				auto DrawPeriod = [this, earliestTimePoint, offset](int index, const ProfileMarker &dataMarker, const ProfileMarker &beginMarker, const ProfileMarker &endMarker, std::chrono::high_resolution_clock::duration excTime = std::chrono::high_resolution_clock::duration())
				{
					std::chrono::high_resolution_clock::duration fromFrameStart = beginMarker.timePoint - earliestTimePoint;
					std::chrono::high_resolution_clock::duration fromFrameEnd = endMarker.timePoint - earliestTimePoint;
					std::chrono::high_resolution_clock::duration markDuration = endMarker.timePoint - beginMarker.timePoint;
					std::chrono::high_resolution_clock::duration inclusiveDuration = endMarker.timePoint - dataMarker.timePoint;
					// show

					auto fromFrameStartMs = std::chrono::duration_cast<std::chrono::duration<float, std::milli>>(fromFrameStart);
					auto fromFrameEndMs = std::chrono::duration_cast<std::chrono::duration<float, std::milli>>(fromFrameEnd);
					auto markDurationMs = std::chrono::duration_cast<std::chrono::duration<float, std::milli>>(markDuration);
					auto exclusiveDurationMs = std::chrono::duration_cast<std::chrono::duration<float, std::milli>>(excTime);
					auto inclusiveDurationMs = std::chrono::duration_cast<std::chrono::duration<float, std::milli>>(inclusiveDuration);

					float beginPosition = fromFrameStartMs.count() > 0 ? fromFrameStartMs.count() * millisecondLength : 0;
					float length = (fromFrameStartMs.count() > 0 ? markDurationMs.count() : fromFrameEndMs.count()) * millisecondLength;

					// cap things at 1 second
					if (beginPosition > 1000 * millisecondLength)
						beginPosition = 1000 * millisecondLength;
					if (length > 1000 * millisecondLength)
						length = 1000 * millisecondLength;
					// cap things at 1 second

					beginPosition += offset;

					if (length < 5.0f)
						length = 5.0f;

					ImGui::SameLine(); // sempre a la mateixa línea de l'anterior

					ImGui::PushID(index);
					// random color. IDLE always red
					float hue = dataMarker.IsIdleMark() ? 0 : ((reinterpret_cast<uintptr_t>(dataMarker.identifier) * 19) % 97) / 97.f; //((beginIndex - profilerNextReadIndex[l] + ProfilerMarkerBufferSize) % ProfilerMarkerBufferSize) * 0.05f; // TODO from job type
					ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(hue, 0.6f, 0.6f));
					ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(hue, 0.7f, 0.7f));
					ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(hue, 0.8f, 0.8f));

					ImGui::SetCursorPosX(beginPosition);
					ImGui::Button(dataMarker.jobName, ImVec2(length, 0.0f));
					if (ImGui::IsItemHovered())
					{
						const char* additionalInfo =
							(endMarker.type == MarkerType::PAUSE_WAIT_FOR_JOB) ? "\nPAUSED waiting dependencies"
							: (endMarker.type == MarkerType::PAUSE_WAIT_FOR_QUEUE_SPACE) ? "\nPAUSED adding jobs"
							: "";

						std::string fullTime = exclusiveDurationMs.count() <= 0 ? "" :
							std::string("\nExclusive Time: ") + std::to_string(exclusiveDurationMs.count()) + "ms"
							+ "\nInclusive Time: " + std::to_string(inclusiveDurationMs.count()) + "ms";

						ImGui::SetTooltip("%s\nbegins: %fms\nends: %fms\nduration: %fms%s%s", dataMarker.jobName, fromFrameStartMs.count(), fromFrameEndMs.count(), markDurationMs.count(), fullTime.c_str(), additionalInfo);
					}
					ImGui::PopStyleColor(3);

					ImGui::PopID();
				};

				// pintem l'ús per cada thread
				for (int l = 0; l < numThreads; l++)
				{
					ImGui::PushID(l);

					ImGui::LabelText("", "core %d", l);

					// pintar les tasques és fàcil: des d'un BEGIN/RESUME fins al primer PAUSE/END que trobem
					for (int beginIndex = profilerNextReadIndex[l]; beginIndex != profilerNextWriteIndex[l]; beginIndex = (beginIndex + 1) % ProfilerMarkerBufferSize)
					{
						const ProfileMarker &beginMarker = profilerData[l][beginIndex];
						if (beginMarker.IsBeginMark())
						{
							for (int endIndex = beginIndex + 1; endIndex != profilerNextWriteIndex[l]; endIndex = (endIndex + 1) % ProfilerMarkerBufferSize)
							{
								const ProfileMarker &endMarker = profilerData[l][endIndex];
								if (beginMarker.identifier == endMarker.identifier && endMarker.IsEndMark())
								{
									DrawPeriod(beginIndex, beginMarker, beginMarker, endMarker);

									beginIndex = endIndex;
									break;
								}
							}
						}
					}

					// pintar les funcions és més complicat doncs cal aniuar-les correctament i tenir en compte que es poden tallar si la tasca s'adorm.		
					// afortunadament una tasca mai es mourà de thread i podem fer servir això per veure el seu nivell
					int depth = 0;
					bool functionFound;
					do
					{
						// la idea és pintar nivell a nivell de funcions fins a arribar a un nivell on no hi hagi cap funció.
						functionFound = false;

						for (int beginIndex = profilerNextReadIndex[l]; beginIndex != profilerNextWriteIndex[l]; beginIndex = (beginIndex + 1) % ProfilerMarkerBufferSize)
						{
							const ProfileMarker &beginMarker = profilerData[l][beginIndex];
							if (beginMarker.type == MarkerType::BEGIN_FUNCTION) // el marcador actual és l'inici d'una funció. Cal dibuixar-la sencera.
							{
								int currentDepth = 0;
								bool inRelevantFiber = true;
								const ProfileMarker *lastInterruption = nullptr;
								// el primer pas és trobar quina profunditat té aquesta funció. 		
								// Per això tirarem enrere fins al BEGIN corresponent a la tasca on és la funció, tallant convenientment 		
								// a les pauses de la tasca i contant quants BEGIN_FUNCTION i END_FUNCTION hi ha
								for (
									int backIndex = (beginIndex == 0) ? ProfilerMarkerBufferSize : (beginIndex - 1);
									(backIndex + 1) % ProfilerMarkerBufferSize != profilerNextReadIndex[l];
									backIndex = (backIndex == 0) ? ProfilerMarkerBufferSize : (backIndex - 1)
									)
								{
									const ProfileMarker &backMarker = profilerData[l][backIndex];
									if (inRelevantFiber)
									{
										if (backMarker.type == MarkerType::BEGIN_FUNCTION)
										{
											++currentDepth;
										}
										else if (backMarker.type == MarkerType::END_FUNCTION)
										{
											--currentDepth;
										}
										else if (backMarker.type == MarkerType::BEGIN)
										{
											//assert(currentDepth >= 0);
											break;
										}
										else if (backMarker.IsBeginMark())
										{
											inRelevantFiber = false;
											lastInterruption = &backMarker;
										}
									}
									else if (backMarker.IsEndMark() && backMarker.identifier == lastInterruption->identifier)
									{
										inRelevantFiber = true;
										lastInterruption = nullptr;
									}
								}

								if (currentDepth == depth) // si la funció és a la profunditat que busquem, la pintem.
								{
									long long exclusiveTime = 0;

									if (!functionFound)
									{
										// pintem una llegenda (i fem una nova línea) si és la primera funció que trobem a aquest nivell.
										ImGui::LabelText("", "stack %d", depth);
										functionFound = true;
									}
									const ProfileMarker *lastInterruptionBegin = nullptr, *lastInterruptionEnd = nullptr, *firstInterruptionBegin = nullptr;
									for (int endIndex = (beginIndex + 1) % ProfilerMarkerBufferSize; endIndex != profilerNextWriteIndex[l]; endIndex = (endIndex + 1) % ProfilerMarkerBufferSize)
									{
										// aquí busquem el END_FUNCTION corresponent a la funció que hem començat.		
										// cada vegada que trobem un tall de la tasca hem de pintar la part anterior d'aquesta funció		
										// i guardar-nos on continua.
										const ProfileMarker &endMarker = profilerData[l][endIndex];
										int idDisc = ProfilerMarkerBufferSize;
										if (endMarker.type == MarkerType::END_FUNCTION && beginMarker.identifier == endMarker.identifier)
										{
											if (lastInterruptionBegin == nullptr)
											{
												DrawPeriod(beginIndex, beginMarker, beginMarker, endMarker);
											}
											else
											{
												assert(lastInterruptionEnd != nullptr);
												assert(firstInterruptionBegin != nullptr);

												exclusiveTime += (endMarker.timePoint - lastInterruptionEnd->timePoint).count();

												DrawPeriod(beginIndex, beginMarker, beginMarker, *firstInterruptionBegin, std::chrono::high_resolution_clock::duration(exclusiveTime));
												ImGui::PushID(idDisc);
												DrawPeriod(endIndex, beginMarker, *lastInterruptionEnd, endMarker, std::chrono::high_resolution_clock::duration(exclusiveTime));
												ImGui::PopID();
											}
											break;
										}
										else if (endMarker.IsBeginMark() && endMarker.identifier == lastInterruptionBegin->identifier)
										{
											lastInterruptionEnd = &endMarker;
										}
										else if (endMarker.IsEndMark() && (lastInterruptionBegin == nullptr || endMarker.identifier == lastInterruptionBegin->identifier))
										{
											if (lastInterruptionBegin != nullptr)
											{
												exclusiveTime += (endMarker.timePoint - lastInterruptionEnd->timePoint).count();

												assert(firstInterruptionBegin != nullptr);
												ImGui::PushID(idDisc);
												DrawPeriod(endIndex, beginMarker, *lastInterruptionEnd, endMarker, std::chrono::high_resolution_clock::duration(exclusiveTime));
												ImGui::PopID();
												++idDisc;
											}
											else
											{
												assert(exclusiveTime == 0);
												exclusiveTime = (endMarker.timePoint - beginMarker.timePoint).count();

												assert(firstInterruptionBegin == nullptr);
												firstInterruptionBegin = &endMarker;
											}
											lastInterruptionBegin = &endMarker;
										}
									}
								}
							}
						}

						++depth;
					} while (functionFound);


					if (recordNewFrame)
						profilerNextReadIndex[l] = profilerNextWriteIndex[l];

					ImGui::PopID();

					ImGui::LabelText("", "-"/*, l*/);
				}
				ImGui::EndChild();
			}
			else if (recordNewFrame)
			{

				for (int l = 0; l < numThreads; l++)
				{
					profilerNextReadIndex[l] = profilerNextWriteIndex[l];  // marquem fins on ha llegit el profiler per no pintar-ho al següent frame.
				}
			}
			ImGui::End();
		}

		// GUARDA. S'aprofita del sistema RAII (Resource acquisition is initialization) de C++
		// i de "move semantics" de C++11 per assegurar-se que es crida el END_FUNCTION 1 vegada.
		struct MarkGuard
		{
			Profiler *profiler;
			int threadIndex;
			const void* identifier;


			MarkGuard(Profiler *_profiler, int _threadIndex, const void* _identifier) : profiler(_profiler), threadIndex(_threadIndex), identifier(_identifier) {}
			MarkGuard(const MarkGuard&) = delete;
			MarkGuard& operator=(const MarkGuard&) = delete;
			MarkGuard(MarkGuard&& other) : profiler(other.profiler), threadIndex(other.threadIndex), identifier(other.identifier) { other.profiler = nullptr; }
			MarkGuard& operator=(MarkGuard&& other) = delete;

			~MarkGuard()
			{
				if (profiler != nullptr)
				{
					profiler->AddProfileMark(MarkerType::END_FUNCTION, identifier, (const char*)nullptr, threadIndex);
				}
			}
		};

	private:

		static constexpr int ProfilerMarkerBufferSize = 16 * 1024;
		static constexpr int MaxNumThreads = 1;

		struct ProfileMarker
		{
			std::chrono::high_resolution_clock::time_point timePoint;
			const void* identifier;
			const char* jobName;
			int systemID;
			MarkerType type;

			bool IsBeginMark() const { return (type == MarkerType::BEGIN || type == MarkerType::RESUME_FROM_PAUSE || type == MarkerType::BEGIN_IDLE); }
			bool IsEndMark() const { return (type == MarkerType::END || type == MarkerType::PAUSE_WAIT_FOR_JOB || type == MarkerType::PAUSE_WAIT_FOR_QUEUE_SPACE || type == MarkerType::END_IDLE); }
			bool IsIdleMark() const { return (type == MarkerType::BEGIN_IDLE || type == MarkerType::END_IDLE); }

		} profilerData[MaxNumThreads][ProfilerMarkerBufferSize];

		int profilerNextWriteIndex[MaxNumThreads] = {}; // TODO do this atomic?
		int profilerNextReadIndex[MaxNumThreads] = {};
		bool recordNewFrame = true;
		float millisecondLength = 400.0f;


	};
}


/*{
	profiler.AddProfileMark(Utilities::Profiler::MarkerType::BEGIN, 0, "Main"); // hem de marcar una tasca per a que el profiler funcioni.

	Update(profiler);

	profiler.AddProfileMark(Utilities::Profiler::MarkerType::END, 0, "Main");
}

void Update()
{
	auto guard = profiler.CreateProfileMarkGuard("Update"); // després afegim marques allà on creguem convenient per a profilejar.

	{
		auto guard = profiler.CreateProfileMarkGuard("Colisions");

		...
	}
}



*/