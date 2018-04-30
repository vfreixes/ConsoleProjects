#pragma once

#include "Game.h"
#include "Common.h"
#include <unordered_map>

#include <vector>
#include <string>
#include <algorithm>

using namespace Game;

GameData *Game::CreateGameData() {

	GameData* gameData = new GameData;
	
	for (int i = 0; i < 5.0f; ++i){
		for (int j = 0; j < 5.0f; ++j) {
			GameObject* ball = new GameObject;
			ball->pos = glm::vec2(-200.0f + i * 50.0f, 100 + j * 50.0f);
			ball->vel = glm::vec2(-40.0f, .0f);
			ball->radi = 20.0f;
			ball->mass = 15.0f;
			gameData->balls.push_back(ball);
		}
	}
	/*GameObject* ball = new GameObject;

	ball->pos = glm::vec2(-200.0f, .0f);
	ball->vel = glm::vec2(-40.0f, .0f);
	ball->radi = 20.0f;
	ball->mass = 15.0f;
	gameData->balls.push_back(ball);

	GameObject* ball2 = new GameObject;

	ball2->pos = glm::vec2(200, .0f);
	ball2->vel = glm::vec2(40.0f, .0f);
	ball2->radi = 20.0f;
	ball2->mass = 15.0f;
	gameData->balls.push_back(ball2);*/
	
	/*for (int i = 0; i < 11; i++)
	{
		ball->pos = glm::vec2(30*i, 100);
		ball->vel = glm::vec2(10*i, 0);
		gameData->balls.push_back(ball);
	}*/
	
	return gameData;
}



void Game::DestroyGameData(GameData* gameData) {
	delete gameData;
}

std::vector<PossibleCollission> Game::SortAndSweep(const std::vector<GameObject*>& gameObjects)
{
	struct Extreme
	{
		GameObject *go;
		float p;
		bool min;
	};

	std::vector<Extreme> list;

	for (int i = 0; i < gameObjects.size(); i++)
	{

		Extreme tmp;
		tmp.go = new GameObject;
		tmp.go->pos = gameObjects[i]->pos;
		tmp.go->radi = gameObjects[i]->radi;
		tmp.go->radi = gameObjects[i]->radi;
		tmp.p = dot(gameObjects[i]->GetExtreme({ -1,0 }), { 1, 0 });
		tmp.min = true;
		list.push_back(tmp);

		tmp.p = dot(gameObjects[i]->GetExtreme({ +1,0 }), { 1, 0 });
		tmp.min = false;
		list.push_back(tmp);

		//list.push_back({ go, dot(go->GetExtreme({ -1,0 }),{ 1, 0 }), true });
		//list.push_back({ go, dot(go->GetExtreme({ +1,0 }),{ 1, 0 }), false });
	}

	std::sort(list.begin(), list.end(), [](const Extreme& first, const Extreme& second)
	{
		if (first.p < second.p)
		{
			return true;
		}
		else
		{
			return false;
		}
	}); // TODO

	std::vector<PossibleCollission> result;

	for (int i = 0; i < list.size(); ++i)
	{
		if (list[i].min)
		{
				for (int j = i + 1;j < list.size(); ++j)
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

std::vector<ContactGroup> Game::GenerateContactGroups(std::vector<ContactData>& contactData)
{
	std::vector<ContactGroup> result;
	std::unordered_map<GameObject*, ContactGroup*> createdGroups;

	result.reserve(contactData.size());

	for (int i = 0; i < contactData.size(); ++i) {
		auto it = createdGroups.find(contactData[i].a);
		if (it == createdGroups.end()) {
			it = createdGroups.find(contactData[i].b);
			if (it == createdGroups.end()) {
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
					for (GameObject* ball : groupB->objects)
					{
						if (ball != contactData[i].a)
							groupA->objects.push_back(ball);
						createdGroups[ball] = groupA;
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


void Game::SolveVelocityAndPenetration(ContactData *contactData) {
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
		glm::vec2 movePerIMass = contactData->normal *(contactData->penetration / totalInvMass);
		contactData->a->pos -= movePerIMass * (1 / (contactData->a->mass));
		contactData->b->pos += movePerIMass * (1 / (contactData->b->mass));
	}


}



void Game::SolveCollissionGroup(const ContactGroup& contactGroup)
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

		/*SolveVelocity(contactData);
		SolvePenetatrion(contactData);*/
		SolveVelocityAndPenetration(contactData);
		/*
		#if ???
		std::vector<PossibleCollission> possibleCollissions;
		for (ContactData& cd : contactGroup.contacts)
		{
		possibleCollissions.push_back({ cd.a, cd.b });
		}

		contacts = FindCollissions(possibleCollissions);
		#elif ???
		contacts = FindCollissions(contactGroup.contacts);
		#else
		// tornem a generar contactes per al grup
		contacts.clear();
		for (int i = 0; i < contactGroup.objects.size(); ++i)
		{
		for (int j = i + 1; j < contactGroup.objects.size(); ++j)
		{
		if (HasCollision(contactGroup.objects[i], contactGroup.objects[j]))
		{
		ContactData cd = GenerateContactData(contactGroup.objects[i], contactGroup.objects[j]);
		contacts.push_back(cd);
		}
		}
		}
		#endif*/
		++iterations;
	}
}



RenderCommands Game::Update(Input const &input, GameData &gameData) {


	gameData.prevBalls = std::move(gameData.balls);
	gameData.balls.clear();

	RenderCommands result = {};
	RenderCommands::Sprite sprite = {};

	//update balls
	int i = 0;
	for (const auto& ball : gameData.prevBalls)
	{
		glm::vec2 f = { 0 , 0 };
		if (i == 0) {
			f.x = input.direction.x;
			f.y = input.direction.y;
		}
		else {

		}
		GameObject* ballNext = new GameObject;
		ballNext->pos = ball->pos + ball->vel * input.dt + f * (0.5f * input.dt * input.dt);
		ballNext->vel = ball->vel + f * input.dt;
		ballNext->radi = ball->radi;
		ballNext->mass = ball->mass;

		if (ballNext->pos.x > input.windowHalfSize.x || ballNext->pos.x < -input.windowHalfSize.x)
			ballNext->vel.x *= -1.0f;
		if (ballNext->pos.y > input.windowHalfSize.y || ballNext->pos.y < -input.windowHalfSize.y)
			ballNext->vel.y *= -1.0f;

		gameData.balls.push_back(ballNext);

		i++;
	}

	//Colisions

	
	std::vector<PossibleCollission> possibleCollisions = SortAndSweep(gameData.balls);
	
	std::vector<ContactData> contactData;

	for (PossibleCollission posCol : possibleCollisions){
		float length = glm::length(posCol.a->pos - posCol.b->pos);
		float sumRad = posCol.a->radi + posCol.b->radi;
		if (length < sumRad) {
			ContactData cd;
			cd.a = posCol.a;
			cd.b = posCol.b;
			cd.point = (cd.a->pos + cd.b->pos) / 2.0f;
			cd.normal = cd.b->pos - cd.a->pos;
			cd.penetration = (cd.a->radi + cd.b->radi) - glm::length(cd.normal);

			cd.restitution = 0.9f;
			cd.friction = 0.1f;

			contactData.push_back(cd);
		}
			
	}

	std::vector<ContactGroup> contactGroups = GenerateContactGroups(contactData);

	for (ContactGroup contGrp : contactGroups)
		SolveCollissionGroup(contGrp);

	//sprites
	for (int i = 0; i < gameData.balls.size(); i++)
	{
		sprite.position = gameData.balls[i]->pos;
		sprite.size = glm::vec2(gameData.balls[i]->radi, gameData.balls[i]->radi);
		if (i == 0) sprite.texture = RenderCommands::TextureNames::PLAYER;
		else sprite.texture = RenderCommands::TextureNames::BALLS;
		result.sprites.push_back(sprite);
	}

	return result;
}