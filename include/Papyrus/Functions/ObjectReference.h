#pragma once

namespace Papyrus::ObjectReference
{
	inline std::vector<RE::TESObjectREFR*> FilterRefArrayByKeywords(IVM& a_vm, VMStackID a_stackID, std::monostate,
		std::vector<RE::TESObjectREFR*> a_refArray,
		std::optional<std::vector<RE::BGSKeyword*>> a_whiteList,
		std::optional<std::vector<RE::BGSKeyword*>> a_blackList)
	{
		std::vector<RE::TESObjectREFR*> result;

		bool shouldAdd{ true };

		if (a_refArray.empty()) {
			a_vm.PostError("Ref array is empty", a_stackID, Severity::kError);
			return result;
		}

		for (const auto currentRef : a_refArray) {
			if (a_whiteList != std::nullopt) {
				for (const auto requiredKW : a_whiteList.value()) {
					if (!currentRef->HasKeyword(requiredKW)) {
						shouldAdd = false;
						break;
					}
				}
			}

			if (a_blackList != std::nullopt) {
				for (const auto bannedKW : a_blackList.value()) {
					if (currentRef->HasKeyword(bannedKW)) {
						shouldAdd = false;
						break;
					}
				}
			}

			if (shouldAdd) {
				result.push_back(currentRef);
			}
		}
		return result;
	}

	inline float GetAnimationLength(IVM& a_vm, VMStackID a_stackID, std::monostate,
		RE::TESObjectREFR* a_ref)
	{
		if (!a_ref) {
			a_vm.PostError("Ref is None", a_stackID, Severity::kError);
			return 0.0f;
		}

		RE::BGSAnimationSystemUtils::ActiveSyncInfo syncInfo;
		// Note from Snapdragon: using GetActiveSyncInfo on the same struct 2 or more times requires clearing
		// the arena first with otherSyncInfo.clear() otherwise you might run into memory manager crashes
		if (!RE::BGSAnimationSystemUtils::IsActiveGraphInTransition(a_ref) && RE::BGSAnimationSystemUtils::GetActiveSyncInfo(a_ref, syncInfo) && syncInfo.totalAnimTime > 0.0f) {
			return syncInfo.totalAnimTime;
		} else {
			return 0.0f;
		}
	}

	inline float GetAnimationTime(IVM& a_vm, VMStackID a_stackID, std::monostate,
		RE::TESObjectREFR* a_ref)
	{
		if (!a_ref) {
			a_vm.PostError("Ref is None", a_stackID, Severity::kError);
			return 0.0f;
		}

		RE::BGSAnimationSystemUtils::ActiveSyncInfo syncInfo;
		if (!RE::BGSAnimationSystemUtils::IsActiveGraphInTransition(a_ref) && RE::BGSAnimationSystemUtils::GetActiveSyncInfo(a_ref, syncInfo) && syncInfo.currentAnimTime > 0.0f) {
			return syncInfo.currentAnimTime;
		} else {
			return 0.0f;
		}
	}

	inline RE::Actor* GetClosestActorFromRef(IVM& a_vm, VMStackID a_stackID, std::monostate,
		RE::TESObjectREFR* a_ref,
		bool a_ignorePlayer,
		bool a_includeDead)
		{
			if (!a_ref) {
				a_vm.PostError("Ref is None", a_stackID, Severity::kError);
				return nullptr;
			}

			auto originPos = a_ref->GetPosition();
			RE::Actor* closestRef = nullptr;
			float smallestDistance = std::numeric_limits<float>::max();
			float currentDistance{ 0.0f };

			const auto processLists = RE::ProcessLists::GetSingleton();
			if (!processLists) {
				a_vm.PostError("Unable to obtain list of Actors", a_stackID, Severity::kError);
				return nullptr;
			}

			for (const auto& actorHandle : processLists->highActorHandles) {
				const auto actorPtr = actorHandle.get();
				const auto currentActor = actorPtr.get();

				if (!currentActor) {
					continue;
				}

				if (!a_includeDead && currentActor->IsDead(true)) {
					continue;
				}

				currentDistance = originPos.GetDistance(currentActor->GetPosition());
				if (currentDistance < smallestDistance) {
					smallestDistance = currentDistance;
					closestRef = currentActor;
				}
			}

			if (!a_ignorePlayer) {
				auto player = RE::PlayerCharacter::GetSingleton();
				if (originPos.GetDistance(player->GetPosition()) < smallestDistance) {
					closestRef = player;
				}
			}

			return closestRef;

	}

	inline RE::TESObjectREFR* GetDoorDestination(IVM& a_vm, VMStackID a_stackID, std::monostate,
		RE::TESObjectREFR* a_ref)
	{
		if (!a_ref) {
			a_vm.PostError("Ref is None", a_stackID, Severity::kError);
			return nullptr;
		}

		const auto xTeleport = a_ref->extraList->GetByType<RE::ExtraTeleport>();
		if (const auto teleportData = xTeleport ? xTeleport->teleportData : nullptr) {
			return teleportData->linkedDoor.get().get();
		}

		return nullptr;
	}

	inline std::vector<RE::TESForm*> GetInventoryItemsAsArray(IVM& a_vm, VMStackID a_stackID, std::monostate,
		RE::TESObjectREFR* a_ref,
		std::vector<bool> a_filterList,
		bool a_matchAll)
	{
		std::vector<RE::TESForm*> result;
		
		if (!a_ref) {
			a_vm.PostError("Ref is None", a_stackID, Severity::kError);
			return result;
		}

		auto refInventory = a_ref->inventoryList->data;
		bool shouldAdd{ false };

		bool shouldFilter{ false };

		// check if any elements are true
		for (size_t i = 0; i < a_filterList.size(); i++) {
			if (a_filterList[i]) {
				// if one is true, then we will need to filter the items
				shouldFilter = true;
				break;
			}
		}

		if (shouldFilter == true) {
			for (auto currentItem : refInventory) {
				shouldAdd = false;
				// equipped
				if (a_filterList.size() > 1 && a_filterList[0]) {
					// check if equipped
					if (currentItem.stackData.get()->IsEquipped()) {
						// if equipped, mark it for adding
						shouldAdd = true;
					} else {
						// if not equipped check if it is required to match all filters
						if (a_matchAll) {
							// can't include it because it was supposed to match all filters
							continue;
						}
					}
				}

				// favourite
				if (a_filterList.size() > 2 && a_filterList[1]) {
					if (currentItem.stackData.get()->extra.get()->IsFavorite()) {
						shouldAdd = true;
					} else {
						if (a_matchAll) {
							continue;
						}
					}
				}

				// legendary
				if (a_filterList.size() > 3 && a_filterList[2]) {
					if (currentItem.stackData.get()->extra.get()->GetLegendaryMod() != nullptr) {
						shouldAdd = true;
					} else {
						if (a_matchAll) {
							continue;
						}
					}
				}

				// multiple
				if (a_filterList.size() > 4 && a_filterList[3]) {
					if (currentItem.stackData.get()->GetCount() > 1) {
						shouldAdd = true;
					} else {
						if (a_matchAll) {
							continue;
						}
					}
				}

				if (shouldAdd) {
					result.push_back((RE::TESForm*)currentItem.object);
				}
			}
			return result;
		}

		// if all elements are false, then we will return all items
		for (auto currentItem : refInventory) {
			result.push_back((RE::TESForm*)currentItem.object);
		}
		return result;
	}

	inline std::vector<RE::BGSKeyword*> GetKeywordsRef(IVM& a_vm, VMStackID a_stackID, std::monostate,
		RE::TESObjectREFR* a_ref)
	{
		std::vector<RE::BGSKeyword*> result;

		if (!a_ref) {
			a_vm.PostError("Ref is None", a_stackID, Severity::kError);
			return result;
		}

		auto refData = a_ref->extraList->GetByType<RE::ExtraInstanceData>();
		if (!refData) {
			a_vm.PostError("Ref data is None", a_stackID, Severity::kError);
			return result;
		}
		auto kwData = refData->data.get()->GetKeywordData();
		if (!kwData) {
			a_vm.PostError("KW data is None", a_stackID, Severity::kError);
			return result;
		}
		int keywordAmount = kwData->GetNumKeywords();
		for (int i = 1; i < (keywordAmount); i++) {
			result.push_back(kwData->GetKeywordAt(i).value());
		}
		return result;
	}

	inline float GetWeightInContainer(IVM& a_vm, VMStackID a_stackID, std::monostate,
		RE::TESObjectREFR* a_ref)
	{
		if (!a_ref) {
			a_vm.PostError("Ref is None", a_stackID, Severity::kError);
			return 0.0f;
		}

		return a_ref->GetWeightInContainer();
	}

	inline bool IsInWater(IVM& a_vm, VMStackID a_stackID, std::monostate,
		RE::TESObjectREFR* a_ref)
	{
		if (!a_ref) {
			a_vm.PostError("Ref is None", a_stackID, Severity::kError);
			return false;
		}

		return a_ref->IsInWater();
	}

	inline bool SetDoorDestination(IVM& a_vm, VMStackID a_stackID, std::monostate,
		RE::TESObjectREFR* a_ref,
		RE::TESObjectREFR* a_door)
	{
		if (!a_ref) {
			a_vm.PostError("Ref is None", a_stackID, Severity::kError);
			return false;
		}
		if (!a_door) {
			a_vm.PostError("Destination Ref is None", a_stackID, Severity::kError);
			return false;
		}

		const auto xTeleport = a_ref->extraList->GetByType<RE::ExtraTeleport>();

		if (const auto teleportData = xTeleport ? xTeleport->teleportData : nullptr) {
			teleportData->linkedDoor = a_door->GetHandle();
			return true;
		}

		return false;
	}

	inline void SetHealthPercent(IVM& a_vm, VMStackID a_stackID, std::monostate,
		RE::TESObjectREFR* a_ref,
		float a_healthPerc)
	{
		if (!a_ref) {
			a_vm.PostError("Ref is None", a_stackID, Severity::kError);
			return;
		}

		if (a_healthPerc < 0 || a_healthPerc > 1) {
			a_vm.PostError("Invalid value. Must not be less than 0 or more than 1", a_stackID, Severity::kError);
			return;
		}

		a_ref->extraList->SetHealthPercent(a_healthPerc);

		return;
	}

	/*
	inline std::vector<RE::Actor*> SortLoadedActorsByDistanceToRef(IVM& a_vm, VMStackID a_stackID, std::monostate,
		RE::TESObjectREFR* a_ref,
		bool a_ignorePlayer,
		bool a_includeDead)
	{
		std::vector<RE::Actor*> result;

		if (!a_ref) {
			a_vm.PostError("Ref is None", a_stackID, Severity::kError);
			return result;
		}

		std::map<RE::Actor*, float> actorMap;
		auto originPos = a_ref->GetPosition();

		if (a_ignorePlayer == false) {
			auto player = RE::PlayerCharacter::GetSingleton();
			actorMap.emplace(player, originPos.GetDistance(player->GetPosition()));
		}

		if (const auto processLists = RE::ProcessLists::GetSingleton(); processLists) {
			auto handleArray = &processLists->highActorHandles;
			for (auto& actorHandle : *handleArray) {
				const auto actorPtr = actorHandle.get();
				if (auto currActor = actorPtr.get(); currActor) {
					if (a_includeDead == true) {
						actorMap.emplace(currActor, originPos.GetDistance(currActor->GetPosition()));
					} else {
						if (currActor->IsDead(true) == false) {
							actorMap.emplace(currActor, originPos.GetDistance(currActor->GetPosition()));
						}
					}
				}
			}
		}

		// TODO: sort the map

		return result;
	}
	*/

	inline void Bind(IVM& a_vm)
	{
		a_vm.BindNativeMethod("Lighthouse", "FilterRefArrayByKeywords", FilterRefArrayByKeywords, true);
		a_vm.BindNativeMethod("Lighthouse", "GetAnimationLength", GetAnimationLength, true);
		a_vm.BindNativeMethod("Lighthouse", "GetAnimationTime", GetAnimationTime, true);
		a_vm.BindNativeMethod("Lighthouse", "GetClosestActorFromRef", GetClosestActorFromRef, true);
		a_vm.BindNativeMethod("Lighthouse", "GetDoorDestination", GetDoorDestination, true);
		a_vm.BindNativeMethod("Lighthouse", "GetInventoryItemsAsArray", GetInventoryItemsAsArray, true);
		//a_vm.BindNativeMethod("Lighthouse", "GetKeywordsRef", GetKeywordsRef, true);
		a_vm.BindNativeMethod("Lighthouse", "GetWeightInContainer", GetWeightInContainer, true);
		a_vm.BindNativeMethod("Lighthouse", "IsInWater", IsInWater, true);
		a_vm.BindNativeMethod("Lighthouse", "SetDoorDestination", SetDoorDestination, true);
		a_vm.BindNativeMethod("Lighthouse", "SetHealthPercent", SetHealthPercent, true);
		//a_vm.BindNativeMethod("Lighthouse", "SortLoadedActorsByDistanceToRef", SortLoadedActorsByDistanceToRef, true);

		logger::info("ObjectReference functions registered.");
	}
}
