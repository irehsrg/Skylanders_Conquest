// Skylanders Conquest - Default sound library implementation

#include "SkylandersSoundLibrary.h"
#include "SkylandersSmokeTest.h"
#include "Sound/SoundBase.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "UObject/ObjectKey.h"

namespace SkylandersSound
{
	USoundBase* Get(const TCHAR* Name)
	{
		if (!Name || !*Name) return nullptr;

		// Weak refs: the cache must not keep sounds alive across a map change, and
		// a stale entry simply reloads.
		static TMap<FName, TWeakObjectPtr<USoundBase>> Cache;
		static TSet<FName> Missing;

		const FName Key(Name);
		if (TWeakObjectPtr<USoundBase>* Found = Cache.Find(Key))
		{
			if (Found->IsValid()) return Found->Get();
		}
		if (Missing.Contains(Key)) return nullptr;

		const FString Path = FString::Printf(TEXT("/Game/Audio/SFX/%s.%s"), Name, Name);
		USoundBase* Sound = LoadObject<USoundBase>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!Sound)
		{
			// Remembered so a missing asset costs one failed load, not one per hit.
			Missing.Add(Key);
			UE_LOG(LogTemp, Verbose, TEXT("SkylandersSound: no asset at %s"), *Path);
			return nullptr;
		}
		Cache.Add(Key, Sound);
		return Sound;
	}

	void PlayAt(const UObject* WorldContext, const TCHAR* Name, const FVector& Location, float Volume, float Pitch)
	{
		if (!WorldContext) return;
		USoundBase* Sound = Get(Name);
		if (!Sound) return;

		if (Pitch == 1.0f && FCString::Strncmp(Name, TEXT("hit_"), 4) == 0)
		{
			Pitch = FMath::FRandRange(0.94f, 1.06f);
		}
		UGameplayStatics::PlaySoundAtLocation(WorldContext, Sound, Location, Volume, Pitch);
		SKY_COUNT("sound_play");
	}

	void Play2D(const UObject* WorldContext, const TCHAR* Name, float Volume)
	{
		if (!WorldContext) return;
		USoundBase* Sound = Get(Name);
		if (!Sound) return;
		UGameplayStatics::PlaySound2D(WorldContext, Sound, Volume);
		SKY_COUNT("sound_play");
	}

	void PlayHit(const UObject* Instigator, const FVector& Location, bool bHeavy)
	{
		if (!Instigator) return;
		const UWorld* World = Instigator->GetWorld();
		if (!World) return;

		// One entry per hero that has landed a hit; tiny in practice (the player).
		static TMap<FObjectKey, double> LastPlay;
		const double Now = World->GetTimeSeconds();
		double& Last = LastPlay.FindOrAdd(FObjectKey(Instigator), -1.0);
		// A new match restarts world time at 0, so a "last" from the future is stale.
		if (Last >= 0.0 && Now >= Last && Now - Last < 0.05) return;
		Last = Now;

		PlayAt(Instigator, bHeavy ? TEXT("hit_heavy") : TEXT("hit_light"), Location);
	}
}
