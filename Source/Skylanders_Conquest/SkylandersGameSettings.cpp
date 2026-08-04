// Skylanders Conquest - Player preferences implementation

#include "SkylandersGameSettings.h"
#include "Misc/App.h"

USkylandersGameSettings* USkylandersGameSettings::Get()
{
	// The CDO loads its Config properties on startup, so it works as the single
	// settings instance without anything having to own or tick it.
	return GetMutableDefault<USkylandersGameSettings>();
}

void USkylandersGameSettings::ApplyAll()
{
	MasterVolume = FMath::Clamp(MasterVolume, 0.0f, 1.0f);
	MouseSensitivity = FMath::Clamp(MouseSensitivity, MinSensitivity, MaxSensitivity);

	FApp::SetVolumeMultiplier(MasterVolume);
}

void USkylandersGameSettings::SaveAndApply()
{
	ApplyAll();
	SaveConfig();
}
