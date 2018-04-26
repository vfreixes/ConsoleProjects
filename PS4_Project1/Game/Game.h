#pragma once

#include <vector>

#include "Common.h"
#include <glm\glm.hpp>

namespace Game {

	struct GameObject
	{
		glm::vec2 pos, vel;
		float radi;

		glm::vec2 GetExtreme(glm::vec2 dir) const { return pos + dir * radi; }
	};


	struct PossibleCollission
	{
		GameObject *a, *b;
	};

	struct ContactData
	{
		struct GameObject *a, *b;
		glm::dvec2 point, normal;
		double penetatrion;
		double restitution, friction;
	};

	struct ContactGroup
	{
		std::vector<GameObject*> objects;
		std::vector<ContactData> contacts;
	};

	struct Line
	{
		glm::dvec2 normal;
		double distance;
	};

	struct GameData
	{
		GameObject player;
		std::vector<GameObject> balls;
		std::vector<GameObject> prevBalls;
		std::vector<PossibleCollission> coll;
	};

	

	

}

