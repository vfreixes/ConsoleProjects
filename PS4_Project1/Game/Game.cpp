#pragma once

#include "Game.h"
#include "Common.h"
#include <unordered_map>

#include <vector>
#include <algorithm>

using namespace Game;

GameData *Game::CreateGameData() {

	GameData* gameData = new GameData;
	
	GameObject* ball = new GameObject;

	ball->pos = glm::vec2(-200.0f, .0f);
	ball->vel = glm::vec2(-40.0f, .0f);
	ball->mass = 15.0f;
	gameData->balls.push_back(ball);

	ball = new GameObject;

	ball->pos = glm::vec2(200, .0f);
	ball->vel = glm::vec2(40.0f, .0f);
	ball->mass = 15.0f;
	gameData->balls.push_back(ball);
	delete ball;
	
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

std::vector<PossibleCollission> SortAndSweep(const std::vector<GameObject*>& gameObjects)
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

	//std::sort(contactData, [](const ContactData& a, const ContactData& b) // aquesta lambda serveix per ordenar i és equivalent a "a < b"
	//{
	//	// ens assegurem que el contacte estigui ben generat
	//	assert(a.a < a.b || a.b == nullptr);
	//	assert(b.a < b.b || b.b == nullptr); // contactes amb l'element "b" a null son contactes amb objectes de massa infinita (parets, pex)
	//	
	//	if(a.a < b.a)
	//		return true;
	//	else if(a.a > b.a)
	//		return false;
	//	else if(a.b < b.b)
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
void SolvePenetatrion(ContactData *contactData) {
	contactData->penetration = (contactData->a->radi + contactData->b->radi) - glm::length(contactData->b->radi - contactData->a->radi);
}

void SolveVelocity(ContactData *contactData) {
	// velocitat d'acostament = a.vel · |b.pos - a.pos|   +   b.vel · |a.pos - b.pos|
	// velocitat d'acostament = -(a.vel - b.vel) · |a.pos - b.pos|
	// velocitat de separació (vs) = (a.vel - b.vel) · |a.pos - b.pos|   =   (a.vel - b.vel) ·  collision.normal

	// conservació del moment (
	// a.mass * a.velocitat + b.mass * b.velocitat == a.mass * a'.velocitat + b.mass * b'.velocitat 
	// vs' = -c * vs
	// c -> coeficient de restitució (0 - 1)


	// impuls = massa * velocitat (Newton * segon)
	// v' = v + invmass * sum(impulsos[i])

	glm::vec2 vs = (contactData->a->vel - contactData->b->vel) * (float)glm::abs(glm::length(contactData->normal));

	vs = -((float)contactData->restitution) * vs;

	glm::vec2 impuls1 = (float)contactData->a->mass * contactData->a->vel;
	glm::vec2 impuls2 = (float)contactData->b->mass * contactData->b->vel;
	glm::vec2 impulsSum = impuls1 + impuls2;

	contactData->a->vel = contactData->a->vel + contactData->a->mass * impulsSum;
	contactData->b->vel = contactData->b->vel + contactData->b->mass * impulsSum;

}

ContactData GenerateContactData(GameObject* ball01, GameObject* ball02) {
	
	ContactData contactData;
	contactData.point = (ball01->pos + ball02->pos) / 2.0f;
	contactData.normal = ball02->pos - ball02->pos;
	contactData.penetration = (ball01->radi + ball02->radi) - glm::length(contactData.normal);

	contactData.restitution = 0.9f;
	contactData.friction = 0.1f;

	return contactData;
}





bool HasCollision(GameObject* ball01, GameObject* ball02) {
	if ( (ball01->radi + ball02->radi) < glm::length(ball02->pos - ball02->pos) ) {
		return true;
	}
	else return false;
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
			if (contactData == nullptr || contactData->penetration < candidate.penetration)
			{
				contactData = &candidate;
			}
		}

		if (contactData->penetration < 1e-5)
		{
			break;
		}

		SolveVelocity(contactData);
		SolvePenetatrion(contactData);

//#if ???
//		std::vector<PossibleCollission> possibleCollissions;
//		for (ContactData& cd : contactGroup.contacts)
//		{
//			possibleCollissions.push_back({ cd.a, cd.b });
//		}
//
//		contacts = FindCollissions(possibleCollissions);
//#elif ???
//		contacts = FindCollissions(contactGroup.contacts);
//#else
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
//#endif
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
		GameObject* ballNext = new GameObject;
		ballNext->pos = ball->pos + ball->vel * input.dt + f * (0.5f * input.dt * input.dt);
		ballNext->vel = ball->vel + f * input.dt;

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
	contactData.reserve(possibleCollisions.size());

	for (int i = 0; i < possibleCollisions.size(); i++){
		contactData[i] = GenerateContactData(possibleCollisions[i].a, possibleCollisions[i].b);
	}

	std::vector<ContactGroup> contactGroups = GenerateContactGroups(contactData);

	for (int i = 0; i < contactGroups.size(); i++)
	{
		SolveCollissionGroup(contactGroups[i]);
	}
	
	

	//sprites
	for (int i = 0; i < gameData.balls.size(); i++)
	{
		sprite.position = gameData.balls[i]->pos;
		sprite.size = glm::vec2(20, 20);
		if (i == 0) sprite.texture = RenderCommands::TextureNames::PLAYER;
		else sprite.texture = RenderCommands::TextureNames::BALLS;
		result.sprites.push_back(sprite);
	}

	return result;
}