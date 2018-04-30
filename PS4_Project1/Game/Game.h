#pragma once

#include <vector>

#include "Common.h"
#include <glm\glm.hpp>

namespace Game {

	struct GameObject
	{
		glm::vec2 pos, vel;
		float radi, mass;

		glm::vec2 GetExtreme(glm::vec2 dir) const { return pos + dir * radi; }
	};


	struct PossibleCollission
	{
		GameObject *a, *b;
	};

	struct ContactData
	{
		struct GameObject *a, *b;
		glm::vec2 point, normal;
		float penetration;
		float restitution, friction;
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
		std::vector<GameObject*> balls;
		std::vector<GameObject*> prevBalls;
		std::vector<PossibleCollission> coll;
	};

	
	std::vector<PossibleCollission> SortAndSweep(const std::vector<GameObject*>& gameObjects);
	std::vector<ContactGroup> GenerateContactGroups(std::vector<ContactData>& contactData);
	void SolveVelocityAndPenetration(ContactData *contactData);
	void SolveCollissionGroup(const ContactGroup& contactGroup);

	

}

