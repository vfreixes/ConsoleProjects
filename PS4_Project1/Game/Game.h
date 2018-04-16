#pragma once

#include <vector>

#include "Common.h"
#include "glm\glm.hpp"

namespace Game {

	struct GameObject
	{
		glm::vec2 position, velocity;
		float radi;

		glm::vec2 GetExtreme(glm::vec2 dir) const { return position + dir * radi; }
	};

	struct GameData
	{
		GameObject Player;
		std::vector<GameObject> Balls;
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

	struct Line
	{
		glm::dvec2 normal;
		double distance;
	};

}

