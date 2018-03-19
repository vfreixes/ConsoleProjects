#pragma once

#include "Game.h"
#include "Common.h"

#include <vector>
#include <algorithm>

using namespace Game;

GameData *Game::CreateGameData() {
	GameData* gameData = new GameData;

	return gameData;
}

RenderCommands Game::Update(Input const &input, GameData &gameData) {

	RenderCommands result = {};


	return result;
}

void Game::DestroyGameData(GameData* gameData) {
	delete gameData;
}

