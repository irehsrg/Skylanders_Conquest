// Skylanders Conquest - Default sound library
//
// Every USoundBase* UPROPERTY on our actors defaults to null and nothing in the
// Blueprints assigns them, so the game was silent. This resolves the shared SFX
// in /Game/Audio/SFX by short name ("hit_light") so C++ can fill those slots at
// BeginPlay (Blueprint-assigned sounds still win) and play one-off cues that
// have no property at all (hit confirms, kills, UI clicks).

#pragma once

#include "CoreMinimal.h"

class USoundBase;
class UObject;

namespace SkylandersSound
{
	// Loads /Game/Audio/SFX/<Name>.<Name> once and caches it. Returns null (and
	// logs once at Verbose) if the asset is missing.
	SKYLANDERS_CONQUEST_API USoundBase* Get(const TCHAR* Name);

	// World-positioned one-shot. "hit_*" sounds get a small random pitch spread
	// when Pitch is left at 1 so rapid hits don't sound machine-gunned.
	SKYLANDERS_CONQUEST_API void PlayAt(const UObject* WorldContext, const TCHAR* Name, const FVector& Location, float Volume = 1.0f, float Pitch = 1.0f);

	// Non-spatialized one-shot (UI, announcer cues).
	SKYLANDERS_CONQUEST_API void Play2D(const UObject* WorldContext, const TCHAR* Name, float Volume = 1.0f);

	// Hit confirm at the target: hit_heavy for big hits, hit_light otherwise.
	// Throttled to one per 0.05 s per instigator so a cone/line ability that hits
	// a whole wave plays one sound, not a dozen stacked ones.
	SKYLANDERS_CONQUEST_API void PlayHit(const UObject* Instigator, const FVector& Location, bool bHeavy);
}
