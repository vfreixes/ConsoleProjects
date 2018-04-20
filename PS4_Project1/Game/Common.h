#pragma once

#include <cinttypes>
#include <vector>
#include <glm\glm.hpp>

namespace Game {
	struct  GameData;

	struct Input
	{
		double dt;
		uint32_t screenWidth, screenHeight;
		bool buttonPressed;
		float dir;
		glm::vec2 direction;
	};

	struct RenderCommands {
		enum class TextureNames {
			PLAYER,
			BALLS,
			TABLE,
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
	RenderCommands Update(Input const &inuput, GameData &gameData);
	void DestroyGameData(GameData* gameData);



}