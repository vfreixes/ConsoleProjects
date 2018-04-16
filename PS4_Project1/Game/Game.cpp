#pragma once

#include "Game.h"
#include "Common.h"

#include <vector>
#include <algorithm>

using namespace Game;

GameData *Game::CreateGameData() {
	GameData* gameData = new GameData;
	gameData->Player.position = glm::vec2(0, 0);
	gameData->Player.velocity = glm::vec2(0, 0);

	GameObject ball;
	ball.position = glm::vec2(0.1, 0.1);
	ball.velocity = glm::vec2(0, 0);
	gameData->Balls.push_back(ball);
	
	return gameData;
}

RenderCommands Game::Update(Input const &input, GameData &gameData) {

	RenderCommands result = {};

	//player
	RenderCommands::Sprite sprite = {};
	sprite.position = gameData.Player.position;
	sprite.size = glm::vec2(20, 20);
	sprite.texture = RenderCommands::TextureNames::PLAYER;

	result.sprites.push_back(sprite);

	//deez nuts
	sprite.position = gameData.Balls[0].position;
	sprite.size = glm::vec2(20, 20);
	sprite.texture = RenderCommands::TextureNames::BALLS;
	result.sprites.push_back(sprite);
	

	return result;
}

void Game::DestroyGameData(GameData* gameData) {
	delete gameData;
}
