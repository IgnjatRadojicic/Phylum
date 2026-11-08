
// Include
#include "objectmanager.h"
#include "instance.h"
#include "modifiersystem.h"
#include "lua.h"
#include "scripttriggervolumes.h"

#include "utils/functions.h"
#include "utils/log.h"

#include <algorithm>

// Game
namespace Game {
	// TriggerVolume
	TriggerVolume::TriggerVolume(ObjectManager& manager, uint32_t id, const glm::vec3& position, float radius) : Object(manager, id, utils::hash_id("SecurityTeleporter.noun")) {
		SetScale(0); // hide the object, effects will still show.
		SetHasCollision(false);
		SetPosition(position);

		mBoundingBox.extent = glm::vec3(radius);
		// mBoundingBox.extent.z = NAN;

		// Remove locomotion, triggers cannot move
		// mLocomotionData.reset();

		if (mNoun) {
			const auto& aidefinition = mNoun->GetAIDefinition();
			if (aidefinition) {
				SetPassiveAbility(mManager.GetGame().GetLua().GetAbility(aidefinition->GetPassiveAbility()));
			}
		}
	}

	void TriggerVolume::OnActivate() {
		if (mPassiveAbility) {
			mPassiveAbility->Activate(std::static_pointer_cast<TriggerVolume>(shared_from_this()));
		}
	}

	void TriggerVolume::OnDeactivate() {
		if (mPassiveAbility) {
			mPassiveAbility->Deactivate(std::static_pointer_cast<TriggerVolume>(shared_from_this()));
		}
	}

	void TriggerVolume::OnTick(float deltaTime) {
		Object::OnTick(deltaTime);
		if (mObjectStates.empty()) {
			return;
		}

		auto radius = mBoundingBox.extent.x;
		for (auto it = mObjectStates.begin(); it != mObjectStates.end(); ++it) {
			bool isInRadius = glm::distance(it->first->GetPosition(), GetPosition()) <= radius;
			if (it->second == 0) {
				if (isInRadius) {
					OnEnter(it->first);
					it->second = 1;
				}
			} else if (isInRadius) {
				OnStay(it->first);
			} else {
				OnExit(it->first);
				it = mObjectStates.erase(it);
			}
		}
	}

	const ObjectPtr& TriggerVolume::GetOwnerObject() const {
		return mOwnerObject;
	}

	void TriggerVolume::Attach(const ObjectPtr& object) {
		mOwnerObject = object;
	}

	void TriggerVolume::Detach(const ObjectPtr& object) {
		if (mOwnerObject == object) {
			mOwnerObject = nullptr;
		}
	}

	void TriggerVolume::AddObject(const ObjectPtr& object) {
		if (object == mOwnerObject) {
			return;
		}

		const auto& [it, inserted] = mObjectStates.try_emplace(object);
		if (inserted) {
			it->second = 0;
		}
	}

	void TriggerVolume::SetOnEnterCallback(sol::protected_function callback) {
		mOnEnter = std::move(callback);
		if (!mEnvironment && mOnEnter) {
			sol::state_view state = mOnEnter.lua_state();
			mEnvironment = sol::environment(state, sol::create, state.globals());
		}
	}

	void TriggerVolume::SetOnExitCallback(sol::protected_function callback) {
		mOnExit = std::move(callback);
		if (!mEnvironment && mOnExit) {
			sol::state_view state = mOnExit.lua_state();
			mEnvironment = sol::environment(state, sol::create, state.globals());
		}
	}

	void TriggerVolume::SetOnStayCallback(sol::protected_function callback) {
		mOnStay = std::move(callback);
		if (!mEnvironment && mOnStay) {
			sol::state_view state = mOnStay.lua_state();
			mEnvironment = sol::environment(state, sol::create, state.globals());
		}
	}

	void TriggerVolume::OnEnter(ObjectPtr object) const {
		if (mOnEnter) {
			mManager.GetGame().GetLua().CallCoroutine<void>(mEnvironment, mOnEnter, shared_from_this(), std::move(object));
		}
	}

	void TriggerVolume::OnExit(ObjectPtr object) const {
		if (mOnExit) {
			mManager.GetGame().GetLua().CallCoroutine<void>(mEnvironment, mOnExit, shared_from_this(), std::move(object));
		}
	}

	void TriggerVolume::OnStay(ObjectPtr object) const {
		if (mOnStay) {
			mManager.GetGame().GetLua().CallCoroutine<void>(mEnvironment, mOnStay, shared_from_this(), std::move(object));
		}
	}

	// ObjectManager
	ObjectManager::ObjectManager(Instance& game) : mGame(game) {
		mOctTree = std::make_unique<OctTree>();
	}

	Instance& ObjectManager::GetGame() {
		return mGame;
	}

	const Instance& ObjectManager::GetGame() const {
		return mGame;
	}

	const std::unordered_set<ObjectPtr>& ObjectManager::GetActiveObjects() const {
		return mActiveObjects;
	}

	ObjectPtr ObjectManager::Get(uint32_t id) const {
		if (auto it = mObjects.find(id); it != mObjects.end()) {
			return it->second;
		}
		return nullptr;
	}

	ObjectPtr ObjectManager::Create(uint32_t noun) {
		uint32_t id = GetNextObjectId();
		if (id == 0) {
			return nullptr;
		}

		const auto& [it, inserted] = mObjects.try_emplace(id);
		if (inserted) {
			// Create a new object
			auto object = ObjectPtr(new Object(*this, id, noun));
			it->second = object;

			// Initialize object (cannot use shared_from_this() when creating the object)
			object->Initialize();

			// Add to octree and active list
			mOctTree->Enqueue(object);
			mActiveObjects.insert(object);

			return std::move(object);
		}

		return nullptr;
	}

	ObjectPtr ObjectManager::Create(const MarkerPtr& marker) {
		if (!marker) {
			return nullptr;
		}

		ObjectPtr object;
		if (const auto& teleporterData = marker->GetTeleporterData()) {
			const auto& triggerVolumeData = teleporterData->GetTriggerVolumeData();
			if (triggerVolumeData) {
				object = CreateTrigger(marker->GetPosition(), triggerVolumeData->GetBoundingBox().extent.x);
			} else {
				object = CreateTrigger(marker->GetPosition(), 1.f);
			}
		} else {
			object = Create(marker->GetNoun());
		}

		if (object) {
			auto overrideAssetId = marker->GetOverrideAssetId();
			if (overrideAssetId > 0) {
				object->SetAssetId(overrideAssetId);
			}

			object->SetPosition(marker->GetPosition());
			object->SetOrientation(marker->GetRotation());
			object->SetMarkerId(marker->GetId());

			if (!object->IsTrigger()) {
				object->SetScale(marker->GetScale());
			}

			const auto& markerInteractableData = marker->GetInteractableData();
			if (markerInteractableData) {
				const auto& interactableData = object->CreateInteractableData();
				interactableData->SetAbility(utils::hash_id(markerInteractableData->GetAbility()));
				interactableData->SetUsesAllowed(markerInteractableData->GetUsesAllowed());
				interactableData->SetTimesUsed(0);
			}

			object->OnActivate();
		}

		return object;
	}

	TriggerVolumePtr ObjectManager::GetTrigger(uint32_t id) const {
		if (auto it = mObjects.find(id); it != mObjects.end()) {
			if (it->second->IsTrigger()) {
				return std::static_pointer_cast<TriggerVolume>(it->second);
			}
		}
		return nullptr;
	}

	TriggerVolumePtr ObjectManager::CreateTrigger(const glm::vec3& position, float radius) {
		uint32_t id = GetNextObjectId();
		if (id == 0) {
			return nullptr;
		}

		const auto& [it, inserted] = mObjects.try_emplace(id);
		if (inserted) {
			auto object = TriggerVolumePtr(new TriggerVolume(*this, id, position, radius));
			it->second = object;

			mOctTree->Enqueue(object);
			mActiveObjects.insert(object);

			return std::move(object);
		}

		return nullptr;
	}

	std::vector<ObjectPtr> ObjectManager::GetObjectsInRegion(const BoundingBox& region, const std::vector<NounType>& types) const {
		return mOctTree->GetObjectsInRegion(region, types);
	}

	std::vector<ObjectPtr> ObjectManager::GetObjectsOfOwner(uint32_t ownerId) const {
		std::vector<ObjectPtr> owned;
		for (const auto& [id, object] : mObjects) {
			if (object && !object->IsMarkedForDeletion()) {
				if (const auto owner = object->GetOwnerObject(); owner && owner->GetId() == ownerId) {
					owned.push_back(object);
				}
			}
		}
		return owned;
	}

	std::vector<ObjectPtr> ObjectManager::GetObjectsInRadius(const BoundingSphere& region, const std::vector<NounType>& types) const {
		return mOctTree->GetObjectsInRadius(region, types);
	}

	// Approximate: how far from a mob's centre a shot still counts, and how far to look for mobs.
	constexpr float smallestTargetRadius = 0.5f;
	constexpr float largestTargetRadius = 15.f;

	std::vector<SweepHit> ObjectManager::SweepObjects(const glm::vec3& from, const glm::vec3& to, const glm::vec3& halfExtent,
		const std::vector<NounType>& types, uint32_t intendedId, const std::function<bool(const ObjectPtr&)>& canHit) const {
		const BoundingSphere area((from + to) * 0.5f, glm::length(to - from) * 0.5f + glm::length(halfExtent) + largestTargetRadius);

		std::vector<SweepTarget> targets;
		for (const auto& object : GetObjectsInRadius(area, types)) {
			if (canHit && !canHit(object)) {
				continue;
			}

			// The centre point is halfway up the mob, so it gives the height of the box.
			const float width = std::max(object->GetFootprintRadius(), smallestTargetRadius);
			const float height = (object->GetCenterPoint().z - object->GetPosition().z) * 2;
			targets.push_back({ object->GetId(), object->GetPosition(), glm::vec3(-width, -width, 0), glm::vec3(width, width, height > 0 ? height : width * 2) });
		}

		return ProjectileSweep(from, to, halfExtent, intendedId).Find(targets);
	}

	bool ObjectManager::IsInLineOfSight(const ObjectPtr& object, const ObjectPtr& target, const glm::vec3& targetPosition) const {
		if (!object) {
			return false;
		}

		const auto& centerPosition = object->GetCenterPoint();

		glm::vec3 testPosition = targetPosition;
		if (target) {
			testPosition = target->GetPosition();
		}

		testPosition.z = (centerPosition.z + testPosition.z) - object->GetPosition().z;

		/*
		if (physicsObject->SomeFunc(centerPoints, &local_34) > 0) {
			return true;
		}
		*/

		// return physicsObject->OtherFunc(&local_44, &stack0xffffffb0, &object->orientation, &stack0xffffffa4, 2, &local_40.z, 1);
		return true;
	}

	void ObjectManager::Update(float deltaTime) {
		mOctTree->Update();
		for (const auto& object : mActiveObjects) {
			object->OnTick(deltaTime);
			if (object->NeedUpdate()) {
				mGame.SendObjectUpdate(object);
			}
		}

		auto* triggerVolumes = mGame.GetTriggerVolumes();
		if (triggerVolumes) {
			triggerVolumes->Update(*this);
		}

		if (!mMarkedObjects.empty()) {
			auto& lua = mGame.GetLua();

			// Before the objects go, because deactivate scripts still reach for them. They can mark
			// more objects too, so the list can grow and move while this runs.
			for (size_t index = 0; index < mMarkedObjects.size(); ++index) {
				const auto object = mMarkedObjects[index];
				// As its physics body goes, so trigger volume scripts still find it.
				if (triggerVolumes) {
					triggerVolumes->OnObjectRemoved(object->GetId());
				}
				mGame.GetModifiers().OnAgentDestroyed(object->GetId());
				// A pet's follow behavior would otherwise wait for it forever.
				object->OnRemoved();
			}

			mGame.SendObjectDelete(mMarkedObjects);
			for (const auto& object : mMarkedObjects) {
				mActiveObjects.erase(object);

				auto id = object->GetId();
				lua.RemovePrivateTable(id);

				mObjects.erase(id);
			}
			mMarkedObjects.clear();

			// Cleanup removed private tables
			lua.CollectGarbage();
		}
	}

	void ObjectManager::MarkForDeletion(const ObjectPtr& object) {
		if (!object->IsMarkedForDeletion()) {
			mMarkedObjects.push_back(object);
		}
	}

	uint32_t ObjectManager::GetNextObjectId() {
		// Ids only count up, as Darkspin's do (zone/allocator.go): a freed id handed out again let a
		// script still holding it (a missile's thread) reach the new object.
		static thread_local uint32_t sNextId = 1;
		return sNextId++;
	}
}
