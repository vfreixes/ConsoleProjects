#pragma once

#include <cinttypes>
#include <vector>
#include <glm\glm.hpp>
#include "Profiler.h"
#include "TaskManager.inl.hpp"
#include "TaskManagerHelpers.hpp"
#include "ThreadsafeStructures.hpp"

namespace Game {
	struct  GameData;



	struct Input
	{
		float dt;
		bool buttonPressed;
		float dir;
		glm::vec2 direction;
		glm::vec2 windowHalfSize;
		bool clicking, clickDown, clickingRight, clickDownRight;
		glm::vec2 mousePosition;
	};

	struct RenderCommands {
		enum class TextureNames {
			PLAYER,
			BALLS,
			IMGUI,
			COUNT
		};

		struct Sprite {
			glm::vec2 position, size;
			float rotation;
			TextureNames texture;
		};

		uint32_t orthoWidth, orthoHeight;
		std::vector<Sprite> sprites;
	};

	GameData* CreateGameData();
	RenderCommands Update(Input const &inuput, GameData &gameData, Utilities::Profiler &profiler, const Utilities::TaskManager::JobContext &context);
	RenderCommands Update(Input const &inuput, GameData &gameData);
	void DestroyGameData(GameData* gameData);



}