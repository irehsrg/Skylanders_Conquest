// Skylanders Conquest - Settings screen

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Scalability.h"
#include "SkylandersSettingsWidget.generated.h"

class UButton;
class USlider;
class UTextBlock;

/** Fired when the screen closes, so whoever opened it can restore itself. */
DECLARE_DELEGATE(FOnSkylandersSettingsClosed);

/**
 * Code-built settings screen, shared by the front-end menu and the in-match
 * pause menu. Video options stage into UGameUserSettings and commit on APPLY;
 * volume and sensitivity take effect immediately and save on close.
 */
UCLASS()
class SKYLANDERS_CONQUEST_API USkylandersSettingsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeOnInitialized() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	FOnSkylandersSettingsClosed OnClosed;

private:
	UPROPERTY() UButton* WindowModeBtn = nullptr;
	UPROPERTY() UButton* ResolutionBtn = nullptr;
	UPROPERTY() UButton* QualityBtn = nullptr;
	UPROPERTY() UButton* VSyncBtn = nullptr;
	UPROPERTY() USlider* VolumeSlider = nullptr;
	UPROPERTY() UTextBlock* VolumeValue = nullptr;
	UPROPERTY() USlider* SensitivitySlider = nullptr;
	UPROPERTY() UTextBlock* SensitivityValue = nullptr;
	UPROPERTY() UTextBlock* StatusText = nullptr;

	/** Selectable resolutions, ascending. Filled from the display's supported modes. */
	TArray<FIntPoint> Resolutions;

	/** True when a video option was changed but APPLY has not been pressed. */
	bool bVideoDirty = false;

	// Video values as of the last APPLY (or of opening the screen). BACK restores
	// these directly: UGameUserSettings::LoadSettings only rolls back properties
	// the ini actually contains, which silently leaves untouched ones changed.
	FIntPoint InitialResolution = FIntPoint::ZeroValue;
	int32 InitialWindowMode = 0;
	bool bInitialVSync = false;
	Scalability::FQualityLevels InitialQuality;

	void CaptureVideoSnapshot();
	void RestoreVideoSnapshot();
	void BuildResolutionList();
	void RefreshLabels();
	void Close();

	UFUNCTION() void OnWindowModeClicked();
	UFUNCTION() void OnResolutionClicked();
	UFUNCTION() void OnQualityClicked();
	UFUNCTION() void OnVSyncClicked();
	UFUNCTION() void OnVolumeChanged(float NewValue);
	UFUNCTION() void OnSensitivityChanged(float NewValue);
	UFUNCTION() void OnApplyClicked();
	UFUNCTION() void OnBackClicked();
};
