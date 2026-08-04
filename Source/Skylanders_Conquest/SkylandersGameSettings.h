// Skylanders Conquest - Player preferences not covered by UGameUserSettings

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "SkylandersGameSettings.generated.h"

/**
 * Audio and look preferences. UGameUserSettings already owns resolution, window
 * mode, quality and vsync, so this only holds what it does not. Values live in
 * the same user-local GameUserSettings.ini, and the class default object doubles
 * as the single instance - no save-game asset required.
 */
UCLASS(Config = GameUserSettings)
class SKYLANDERS_CONQUEST_API USkylandersGameSettings : public UObject
{
	GENERATED_BODY()

public:
	static constexpr float MinSensitivity = 0.25f;
	static constexpr float MaxSensitivity = 3.0f;

	/** 0..1 multiplier applied to all game audio. */
	UPROPERTY(Config, BlueprintReadOnly, Category = "Audio")
	float MasterVolume = 1.0f;

	/** Multiplier applied to mouse look input. */
	UPROPERTY(Config, BlueprintReadOnly, Category = "Input")
	float MouseSensitivity = 1.0f;

	static USkylandersGameSettings* Get();

	/** Clamps the stored values and pushes them into the engine. */
	void ApplyAll();

	/** ApplyAll, then write to disk. */
	void SaveAndApply();
};
