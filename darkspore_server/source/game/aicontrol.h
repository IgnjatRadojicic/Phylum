
#ifndef _GAME_AICONTROL_HEADER
#define _GAME_AICONTROL_HEADER

// Game
namespace Game {
	class Object;

	// A hero: player controlled, or a player class's creature (the hero sweep's are not controlled).
	bool IsHero(const Object& object);

	// Something a hero's ability made, such as a pet, a turret or a sprite: the scripts make the hero
	// its owner (SetOwnerID), or the owner of whatever made it. Being on the heroes' team is not
	// enough, as the level's friendly creatures are on it too.
	bool IsHeroSummon(const Object& object);

	// Whether the object's AI runs its gambits: a hero summon always, any other NPC only with
	// AI_MOBS on in the server config.
	bool RunsGambits(const Object& object);
}

#endif
