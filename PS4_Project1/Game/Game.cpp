#include "Game.h"
#include "Common.h"

using namespace Game;

GameData *Game::CreateGameData() {
	return new GameData;
}

RenderCommands Game::Update(Input const &input, GameData &gameData) {

	RenderCommands result = {};


	return result;
}

void Game::DestroyGameData(GameData* gameData) {
	delete gameData;
}