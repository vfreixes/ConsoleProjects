#pragma once

#include "Game.h"
#include "Common.h"

#include <vector>
#include <algorithm>

using namespace Game;

GameData *Game::CreateGameData() {

	GameData* gameData = new GameData;
	
	GameObject ball;
	
	for (int i = 0; i < 11; i++)
	{
		ball.pos = glm::vec2(30*i, 100);
		ball.vel = glm::vec2(1, 0);
		gameData->balls.push_back(ball);
	}
	
	return gameData;
}

RenderCommands Game::Update(Input const &input, GameData &gameData) {


	gameData.prevBalls = std::move(gameData.balls);
	gameData.balls.clear();

	RenderCommands result = {};
	RenderCommands::Sprite sprite = {};
	////player
	//gameData.balls[0].pos += gameData.balls[0].vel;
	
	//sprite.position = gameData.balls[0].pos;
	//sprite.size = glm::vec2(20, 20);
	//sprite.texture = RenderCommands::TextureNames::PLAYER;

	//result.sprites.push_back(sprite);

	
	//update balls
	for (const auto& ball : gameData.prevBalls)
	{
		glm::vec2 f = { 0.9, 0.9 };
		GameObject ballNext = {};
		ballNext.pos = ball.pos + ball.vel * input.dt + f * (0.5f * input.dt * input.dt);
		ballNext.vel = ball.vel + f * input.dt;

		if (ballNext.pos.x > input.windowHalfSize.x || ballNext.pos.x < -input.windowHalfSize.x)
			ballNext.vel.x = -ballNext.vel.x;
		if (ballNext.pos.y > input.windowHalfSize.y || ballNext.pos.y < -input.windowHalfSize.y)
			ballNext.vel.y = -ballNext.vel.y;

		gameData.balls.push_back(ballNext);

		
	}
	

	//sprites
	for (int i = 0; i < 11; i++)
	{
		sprite.position = gameData.balls[i].pos;
		sprite.size = glm::vec2(20, 20);
		if( i == 0) sprite.texture = RenderCommands::TextureNames::PLAYER;
		else sprite.texture = RenderCommands::TextureNames::BALLS;
		result.sprites.push_back(sprite);
	}

	return result;
}

void Game::DestroyGameData(GameData* gameData) {
	delete gameData;
}

