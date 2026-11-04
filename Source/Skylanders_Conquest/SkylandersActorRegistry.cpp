// Skylanders Conquest - Actor Registry Implementation

#include "SkylandersActorRegistry.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "SkylandersBuffCamp.h"
#include "SkylandersEnemy.h"
#include "SkylandersEnemyGod.h"
#include "SkylandersMinion.h"
#include "SkylandersSpawnArea.h"
#include "SkylandersTitan.h"
#include "SkylandersTower.h"

void USkylandersActorRegistry::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// The set of classes the game queries by type. Add a class here and have
	// it call Register/Unregister; nothing else needs to change.
	Buckets.Add(ASkylandersMinion::StaticClass());
	Buckets.Add(ASkylandersEnemy::StaticClass());
	Buckets.Add(ASkylandersEnemyGod::StaticClass());
	Buckets.Add(ASkylandersTower::StaticClass());
	Buckets.Add(ASkylandersTitan::StaticClass());
	Buckets.Add(ASkylandersBuffCamp::StaticClass());
	Buckets.Add(ASkylandersSpawnArea::StaticClass());
}

USkylandersActorRegistry* USkylandersActorRegistry::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}
	UWorld* World = WorldContextObject->GetWorld();
	return World ? World->GetSubsystem<USkylandersActorRegistry>() : nullptr;
}

void USkylandersActorRegistry::GetAllActorsOfClass(const UObject* WorldContextObject, TSubclassOf<AActor> ActorClass, TArray<AActor*>& OutActors)
{
	OutActors.Reset();

	USkylandersActorRegistry* Registry = Get(WorldContextObject);
	const TArray<TWeakObjectPtr<AActor>>* Bucket = Registry ? Registry->Buckets.Find(ActorClass.Get()) : nullptr;
	if (!Bucket)
	{
		// Untracked class (or no world yet): keep the old behaviour.
		UGameplayStatics::GetAllActorsOfClass(WorldContextObject, ActorClass, OutActors);
		return;
	}

	OutActors.Reserve(Bucket->Num());
	for (const TWeakObjectPtr<AActor>& Weak : *Bucket)
	{
		if (AActor* Actor = Weak.Get())
		{
			if (IsValid(Actor))
			{
				OutActors.Add(Actor);
			}
		}
	}
}

void USkylandersActorRegistry::Register(AActor* Actor)
{
	if (!Actor)
	{
		return;
	}
	for (auto& Pair : Buckets)
	{
		if (Actor->IsA(Pair.Key))
		{
			Pair.Value.AddUnique(Actor);
		}
	}
}

void USkylandersActorRegistry::Unregister(AActor* Actor)
{
	if (!Actor)
	{
		return;
	}
	for (auto& Pair : Buckets)
	{
		Pair.Value.RemoveSingleSwap(Actor);
	}
}
