// Skylanders Conquest - Actor Registry
//
// Per-world index of the gameplay actors that AI, towers, projectiles and
// widgets query every tick (minions, gods, towers, titans, camps, spawn areas).
// Replaces the per-actor-per-tick UGameplayStatics::GetAllActorsOfClass scans
// flagged in docs/AUDIT-2026-08-02.md: a lookup here is a copy of a small
// array instead of a walk over every actor in the world.
//
// Actors register in BeginPlay and unregister in EndPlay. Anything not tracked
// here (engine classes such as APlayerStart) falls through to the engine scan,
// so the call is always correct, just not always fast.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SkylandersActorRegistry.generated.h"

UCLASS()
class SKYLANDERS_CONQUEST_API USkylandersActorRegistry : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// Drop-in replacement for UGameplayStatics::GetAllActorsOfClass. Returns
	// live (non-pending-kill) tracked actors of ActorClass, or defers to the
	// engine scan for classes this registry does not index.
	static void GetAllActorsOfClass(const UObject* WorldContextObject, TSubclassOf<AActor> ActorClass, TArray<AActor*>& OutActors);

	static USkylandersActorRegistry* Get(const UObject* WorldContextObject);

	// Called from the tracked classes' BeginPlay / EndPlay.
	void Register(AActor* Actor);
	void Unregister(AActor* Actor);

	bool IsTrackedClass(const UClass* ActorClass) const { return Buckets.Contains(ActorClass); }

private:
	// One bucket per tracked C++ base class. An actor lands in every bucket
	// whose class it IsA, so subclasses are found through their base.
	TMap<const UClass*, TArray<TWeakObjectPtr<AActor>>> Buckets;
};
