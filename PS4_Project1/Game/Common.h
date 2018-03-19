#pragma once

#include <cinttypes>
#include <vector>

namespace Game {
	struct  GameData;

	struct Input
	{
		double dt;
		uint32_t screenWidth, screenHeight;
		bool buttonPressed;
		float dir;
	};

	struct RenderCommands {
		struct Sprite {
			float x, y, width, height, rotation;
			enum
			{
				BALL, POINT_1, POINT_2, PLAYER
			} image;
		};

		uint32_t orthoWidth, orthoHeight;
		std::vector<Sprite> sprites;
	};

	GameData* CreateGameData();
	RenderCommands Update(Input const &inuput, GameData &gameData);
	void DestroyGameData(GameData* gameData);



}