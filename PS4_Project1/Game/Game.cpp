#pragma once

#include "Game.h"
#include "Common.h"
#include <unordered_map>

#include <vector>
#include <algorithm>

#define BALLS_MAX_X 100
#define BALLS_MAX_Y 50
#define K0 0.99f
using namespace Game;
using namespace Utilities;

GameData *Game::CreateGameData() {

	GameData* gameData = new GameData;
	
	GameObject* ball;
	
	for (int i = 0; i < BALLS_MAX_X; ++i)
	{
		for (int j = 0; j < BALLS_MAX_Y; ++j)
		{
			ball = new GameObject;
			ball->pos = glm::vec2(-750 + ((750 * 2) / BALLS_MAX_X) * i, -400 + ((800 * 2) / BALLS_MAX_Y) * j);
			ball->vel = glm::vec2(-40, 0);
			ball->mass = 1;
			ball->invMass = 1 / ball->mass;
			ball->radi = 2;
			gameData->balls.push_back(ball);
		}
	}
	
	return gameData;
}

void Game::DestroyGameData(GameData* gameData) {
	delete gameData;
}


std::vector<PossibleCollision> SortAndSweep(const std::vector<GameObject*>& gameObjects)
{
	struct Extreme
	{
		GameObject *go;
		float p;
		bool min;
	};

	std::vector<Extreme> list;

	for (GameObject* go : gameObjects)
	{

		Extreme tmp = {}, tmp2 = {};
		tmp.go = new GameObject;
		tmp.go = go;
		tmp.p = dot(go->GetExtreme({ -1,0 }), { 1, 0 });
		tmp.min = true;
		list.push_back(tmp);
		
		tmp2.go = new GameObject;
		tmp2.go = go;
		tmp2.p = dot(go->GetExtreme({ +1,0 }), { 1, 0 });
		tmp2.min = false;
		list.push_back(tmp2);

	}

	std::sort(list.begin(), list.end(), [](const Extreme& first, const Extreme& second){
		return first.p < second.p;		
	});
	std::vector<PossibleCollision> result;

	for (int i = 0; i < list.size(); ++i)
	{
		if (list[i].min)
		{
			for (int j = i + 1; list[i].go != list[j].go; ++j)
			{
				if (list[j].min)
				{
					result.push_back({ list[i].go, list[j].go });
				}
			}
		}
	}
	return result;
}

std::vector<ContactGroup> GenerateContactGroups(std::vector<ContactData> contactData)
{
	// si ordenem podem "simplificar" la segona part.
	// sense probar-ho no podem saber si és més optim o no.

	//std::sort(contactData.begin(), contactData.end(), [](const ContactData& a, const ContactData& b) // aquesta lambda serveix per ordenar i és equivalent a "a < b"
	//{
	//	// ens assegurem que el contacte estigui ben generat
	//	assert(a.a < a.b || a.b == nullptr);
	//	assert(b.a < b.b || b.b == nullptr); // contactes amb l'element "b" a null son contactes amb objectes de massa infinita (parets, pex)

	//	if (a.a < b.a)
	//		return true;
	//	else if (a.a > b.a)
	//		return false;
	//	else if (a.b < b.b)
	//		return true;
	//	else
	//		return false;
	//});

	std::vector<ContactGroup> result;
	std::unordered_map<GameObject*, ContactGroup*> createdGroups;

	result.reserve(contactData.size());

	for (int i = 0; i < contactData.size(); ++i)
	{
		auto it = createdGroups.find(contactData[i].a); // busquem si ja tenim alguna colisió amb l'objecte "a"
		if (it == createdGroups.end())
		{
			it = createdGroups.find(contactData[i].b); // busquem si ja tenim alguna colisió amb l'objecte "b"

			if (it == createdGroups.end())
			{
				ContactGroup group;
				group.objects.push_back(contactData[i].a);
				group.objects.push_back(contactData[i].b);
				group.contacts.push_back(contactData[i]);

				result.push_back(group); // creem la llista de colisions nova

				createdGroups[contactData[i].a] = &result[result.size() - 1]; // guardem referència a aquesta llista
				createdGroups[contactData[i].b] = &result[result.size() - 1]; // per cada objecte per trobarla
			}
			else
			{
				it->second->contacts.push_back(contactData[i]); // afegim la colisió a la llista
				it->second->objects.push_back(contactData[i].a);
				createdGroups[contactData[i].a] = it->second; // guardem referència de l'objecte que no hem trobat abans
			}
		}
		else
		{
			ContactGroup *groupA = it->second;
			groupA->contacts.push_back(contactData[i]);

			auto itB = createdGroups.find(contactData[i].b);
			if (itB == createdGroups.end())
			{
				groupA->objects.push_back(contactData[i].b);
				createdGroups[contactData[i].b] = groupA;
			}
			else
			{
				ContactGroup *groupB = itB->second;

				if (groupA != groupB)
				{
					// el objecte b ja és a un grup diferent, fem merge dels 2 grups.

					// 1 - copiem tot el grup anterior
					for (auto& contactData : groupB->contacts)
					{
						groupA->contacts.push_back(contactData);
					}

					// 2 - copiem els elements del segon grup i actualitzem el mapa
					//     de grups
					for (GameObject* go : groupB->objects)
					{
						if (go != contactData[i].a)
							groupA->objects.push_back(go);
						createdGroups[go] = groupA;
					}

					// 3 - marquem el grup com a buit
					groupB->objects.clear();
					groupB->contacts.clear();
				}
			}
		}
	}

	return result;
}

void SolveVelocityAndPenetration(ContactData *contactData) {
	// velocitat d'acostament = a.vel · |b.pos - a.pos|   +   b.vel · |a.pos - b.pos|
	// velocitat d'acostament = -(a.vel - b.vel) · |a.pos - b.pos|
	// velocitat de separació (vs) = (a.vel - b.vel) · |a.pos - b.pos|   =   (a.vel - b.vel) ·  collision.normal

	// conservació del moment (
	// a.mass * a.velocitat + b.mass * b.velocitat == a.mass * a'.velocitat + b.mass * b'.velocitat 
	// vs' = -c * vs
	// c -> coeficient de restitució (0 - 1)


	// impuls = massa * velocitat (Newton * segon)
	// v' = v + invmass * sum(impulsos[i])

	//-calcular velocitat de separació(Vs)
	//	// - si Vs > 0
	//	//   - novaVs = -c * Vs
	//	//   - totalInvMass = invMass[0] + invMass[1]
	//	//   - deltaV = novaVs - Vs
	//	//   - impuls = deltaV / totalInvMass
	//	//   - impulsPerIMass = contactNormal * impuls
	//	//   - novaVelA = velA + impulsPerIMass * invMassA
	//	//   - novaVelB = velB - impulsPerIMass * invMassB

	float vs = glm::dot((contactData->a->vel - contactData->b->vel), contactData->normal);

	float vsp = .0f; float totalInvMass = .0f; float deltaV = .0f; float impuls = .0f; glm::vec2 impulsPerIMass;
	if (vs > .0f) {
		vsp = -(contactData->restitution) * vs;
		totalInvMass = 1 / contactData->a->mass + 1 / contactData->b->mass;
		deltaV = vsp - vs;
		impuls = deltaV / totalInvMass;
		impulsPerIMass = contactData->normal * impuls;
		contactData->a->vel += impulsPerIMass * (1 / contactData->a->mass);
		contactData->b->vel -= impulsPerIMass * (1 / contactData->b->mass);
	}

	// resoldre interpendetració

	// desplaçament A + desplaçament B == interpendetració (al llarg de la normal)
	// massaA * deltaA == massaB * deltaB

	// deltaA =   contactNormal * interpendetració * massaB / (massaA + massaB)
	// deltaB = - contactNormal * interpendetració * massaA / (massaA + massaB)


	// resum:
	// - si interpendetració > 0
	//   - totalInvMass = invMass[0] + invMass[1]
	//   - movePerIMass = contactNormal * (interpendetració / totalInvMass)
	//   - movimentA =   movePerIMass * invMassA
	//   - movimentB = - movePerIMass * invMassB
	contactData->penetration = -glm::length((contactData->a->pos - contactData->b->pos)) + contactData->a->radi + contactData->b->radi;
	if (contactData->penetration > 0) {
		totalInvMass = 1 / contactData->a->mass + 1 / contactData->b->mass;
		glm::vec2 movePerIMass = contactData->normal * (float)(contactData->penetration / totalInvMass);
		contactData->a->pos -= movePerIMass * (1 / (contactData->a->mass));
		contactData->b->pos += movePerIMass * (1 / (contactData->b->mass));
	}


}

void SolveCollissionGroup(const ContactGroup& contactGroup)
{
	std::vector<ContactData> contacts = contactGroup.contacts;
	int iterations = 0;
	while (contacts.size() > 0 && iterations < contactGroup.contacts.size() * 3)
	{
		// busquem la penetració més gran
		ContactData* contactData = nullptr;
		for (ContactData& candidate : contacts)
		{
			
			candidate.penetration = glm::length((candidate.a->pos + candidate.a->radi) - (candidate.b->pos + candidate.b->radi));
			if (contactData == nullptr || contactData->penetration < candidate.penetration)
			{
				contactData = &candidate;
			}
		}

		if (contactData->penetration < 1e-5)
		{
			break;
		}

		SolveVelocityAndPenetration(contactData);


		++iterations;
	}
}

ContactData GenerateContactData(GameObject* cg1, GameObject* cg2) {  


	ContactData tmp = {};
	tmp.a = cg1;
	tmp.b = cg2;
	tmp.normal = glm::normalize(cg1->pos - cg2->pos);
	tmp.point = glm::vec2(cg1->pos + cg2->pos) / 2.0f;
	tmp.friction = 1;
	tmp.restitution = 1;

	return tmp;
}


//Update windows
RenderCommands Game::Update(Input const &input, GameData &gameData, Utilities::Profiler &profiler) {

	const float k0 = K0; // % de velocitat que es manté cada segon

	float brakeValue = pow(k0, input.dt);

	gameData.prevBalls = std::move(gameData.balls);
	gameData.balls.clear();

	RenderCommands result = {};
	RenderCommands::Sprite sprite = {};

	//update balls

	profiler.AddProfileMark(Utilities::Profiler::MarkerType::BEGIN, 0, "BallPositions");
	profiler.AddProfileMark(Utilities::Profiler::MarkerType::BEGIN_FUNCTION, 0, "movement");

	int i = 0;
	for (const auto& ball : gameData.prevBalls)
	{
		glm::vec2 f = { 0 , 0 };
		
		//player
		if (i == 0) {
			
			f.x = input.direction.x;
			f.y = input.direction.y;
			
		}
		glm::vec2 a = f * ball->invMass;

		GameObject* ballNext = new GameObject;
		ballNext->pos = ball->pos + ball->vel * input.dt + a *(0.5f * input.dt * input.dt);
		ballNext->vel = brakeValue * ball->vel + a * input.dt;
		ballNext->radi = ball->radi;
		ballNext->mass = ball->mass;
		ballNext->invMass = ball->invMass;

		if (ballNext->pos.x > input.windowHalfSize.x || ballNext->pos.x < -input.windowHalfSize.x)
			ballNext->vel.x = -ballNext->vel.x;
		if (ballNext->pos.y > input.windowHalfSize.y || ballNext->pos.y < -input.windowHalfSize.y)
			ballNext->vel.y = -ballNext->vel.y;

		gameData.balls.push_back(ballNext);
		

		i++;
	}

	profiler.AddProfileMark(Utilities::Profiler::MarkerType::END_FUNCTION, 0, "movement");
	profiler.AddProfileMark(Utilities::Profiler::MarkerType::END, 0, "BallPositions");


	profiler.AddProfileMark(Utilities::Profiler::MarkerType::BEGIN, 0, "BallCollision");
	profiler.AddProfileMark(Utilities::Profiler::MarkerType::BEGIN_FUNCTION, 0, "colision");
	std::vector<PossibleCollision> possibleCollisions = SortAndSweep(gameData.balls);
	
	std::vector<ContactData> contactData;

	for (PossibleCollision coll : possibleCollisions) {

		double distance, radius;
		distance = glm::distance(coll.a->pos, coll.b->pos);
		radius = coll.a->radi + coll.b->radi;
		if (distance < radius) {
			ContactData tmpCD;
			tmpCD = GenerateContactData(coll.a, coll.b);
			contactData.push_back(tmpCD);
		}
		
	}

	std::vector<ContactGroup> contactGroup = GenerateContactGroups(contactData);

	for (ContactGroup group : contactGroup) {
		SolveCollissionGroup(group);
	}
	profiler.AddProfileMark(Utilities::Profiler::MarkerType::END_FUNCTION, 0, "colision");
	profiler.AddProfileMark(Utilities::Profiler::MarkerType::END, 0, "BallCollision");
	
	//sprites

	profiler.AddProfileMark(Utilities::Profiler::MarkerType::BEGIN, 0, "UpdateSprite");
	profiler.AddProfileMark(Utilities::Profiler::MarkerType::BEGIN_FUNCTION, 0, "sprites");
	for (int i = 0; i < gameData.balls.size(); i++)
	{
		sprite.position = gameData.balls[i]->pos;
		sprite.size = glm::vec2(gameData.balls[i]->radi * 2, gameData.balls[i]->radi * 2);
		if (i == 0) sprite.texture = RenderCommands::TextureNames::PLAYER;
		else sprite.texture = RenderCommands::TextureNames::BALLS;
		result.sprites.push_back(sprite);
	}
	profiler.AddProfileMark(Utilities::Profiler::MarkerType::END_FUNCTION, 0, "sprites");
	profiler.AddProfileMark(Utilities::Profiler::MarkerType::END, 0, "UpdateSprite");
	

	return result;
}


//Update ps4, no profiler
RenderCommands Game::Update(Input const &input, GameData &gameData) {

	const float k0 = K0; // % de velocitat que es manté cada segon

	float brakeValue = pow(k0, input.dt);

	gameData.prevBalls = std::move(gameData.balls);
	gameData.balls.clear();

	RenderCommands result = {};
	RenderCommands::Sprite sprite = {};

	//update balls

	int i = 0;
	for (const auto& ball : gameData.prevBalls)
	{
		glm::vec2 f = { 0 , 0 };

		//player
		if (i == 0) {

			f.x = input.direction.x;
			f.y = input.direction.y;

		}
		glm::vec2 a = f * ball->invMass;

		GameObject* ballNext = new GameObject;
		ballNext->pos = ball->pos + ball->vel * input.dt + a * (0.5f * input.dt * input.dt);
		ballNext->vel = brakeValue * ball->vel + a * input.dt;
		ballNext->radi = ball->radi;
		ballNext->mass = ball->mass;
		ballNext->invMass = ball->invMass;

		if (ballNext->pos.x > input.windowHalfSize.x || ballNext->pos.x < -input.windowHalfSize.x)
			ballNext->vel.x = -ballNext->vel.x;
		if (ballNext->pos.y > input.windowHalfSize.y || ballNext->pos.y < -input.windowHalfSize.y)
			ballNext->vel.y = -ballNext->vel.y;

		gameData.balls.push_back(ballNext);


		i++;
	}

	std::vector<PossibleCollision> possibleCollisions = SortAndSweep(gameData.balls);

	std::vector<ContactData> contactData;

	for (PossibleCollision coll : possibleCollisions) {

		double distance, radius;
		distance = glm::distance(coll.a->pos, coll.b->pos);
		radius = coll.a->radi + coll.b->radi;
		if (distance < radius) {
			ContactData tmpCD;
			tmpCD = GenerateContactData(coll.a, coll.b);
			contactData.push_back(tmpCD);
		}

	}

	std::vector<ContactGroup> contactGroup = GenerateContactGroups(contactData);

	for (ContactGroup group : contactGroup) {
		SolveCollissionGroup(group);
	}
	
	//sprites

	for (int i = 0; i < gameData.balls.size(); i++)
	{
		sprite.position = gameData.balls[i]->pos;
		sprite.size = glm::vec2(gameData.balls[i]->radi * 2, gameData.balls[i]->radi * 2);
		if (i == 0) sprite.texture = RenderCommands::TextureNames::PLAYER;
		else sprite.texture = RenderCommands::TextureNames::BALLS;
		result.sprites.push_back(sprite);
	}
	
	return result;
}




