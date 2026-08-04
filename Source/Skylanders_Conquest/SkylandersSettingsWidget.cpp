// Skylanders Conquest - Settings screen implementation

#include "SkylandersSettingsWidget.h"
#include "SkylandersUIStyle.h"
#include "SkylandersGameSettings.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Slider.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Blueprint/WidgetTree.h"
#include "GameFramework/GameUserSettings.h"
#include "Kismet/KismetSystemLibrary.h"
#include "InputCoreTypes.h"

// ---------- Value formatting ----------

static FString WindowModeName(EWindowMode::Type Mode)
{
	switch (Mode)
	{
	case EWindowMode::Fullscreen:         return TEXT("FULLSCREEN");
	case EWindowMode::WindowedFullscreen: return TEXT("BORDERLESS");
	default:                              return TEXT("WINDOWED");
	}
}

static EWindowMode::Type NextWindowMode(EWindowMode::Type Mode)
{
	switch (Mode)
	{
	case EWindowMode::Fullscreen:         return EWindowMode::WindowedFullscreen;
	case EWindowMode::WindowedFullscreen: return EWindowMode::Windowed;
	default:                              return EWindowMode::Fullscreen;
	}
}

static FString QualityName(int32 Level)
{
	switch (Level)
	{
	case 0:  return TEXT("LOW");
	case 1:  return TEXT("MEDIUM");
	case 2:  return TEXT("HIGH");
	case 3:  return TEXT("EPIC");
	case 4:  return TEXT("CINEMATIC");
	default: return TEXT("CUSTOM"); // GetOverallScalabilityLevel returns -1 when groups differ
	}
}

// ---------- Build ----------

void USkylandersSettingsWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (!WidgetTree) return;

	// Escape should back out of this screen, which needs keyboard focus.
	SetIsFocusable(true);

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("SettingsRoot"));
	WidgetTree->RootWidget = Root;

	// Dimmed backdrop - opaque enough to read over gameplay, since the pause
	// menu opens this screen on top of a live level.
	UBorder* BG = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SettingsBG"));
	BG->SetBrushColor(SkylandersUI::OverlayBG);
	UCanvasPanelSlot* BGSlot = Root->AddChildToCanvas(BG);
	BGSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	BGSlot->SetOffsets(FMargin(0));

	UTextBlock* Title = SkylandersUI::MakeText(WidgetTree, TEXT("SettingsTitle"), TEXT("SETTINGS"), 42, SkylandersUI::Gold);
	UCanvasPanelSlot* TitleSlot = Root->AddChildToCanvas(Title);
	TitleSlot->SetAnchors(FAnchors(0.5f, 0.12f, 0.5f, 0.12f));
	TitleSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	TitleSlot->SetAutoSize(true);

	// Panel holding the rows
	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SettingsPanel"));
	Panel->SetBrushColor(SkylandersUI::PanelBG);
	Panel->SetPadding(FMargin(28.f, 24.f));
	UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel);
	PanelSlot->SetAnchors(FAnchors(0.5f, 0.55f, 0.5f, 0.55f));
	PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	PanelSlot->SetSize(FVector2D(640.f, 470.f));
	PanelSlot->SetAutoSize(false);

	UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SettingsRows"));
	Panel->AddChild(Box);

	auto AddRow = [&](UWidget* W, float VPad = 5.f)
	{
		UVerticalBoxSlot* S = Box->AddChildToVerticalBox(W);
		S->SetPadding(FMargin(0.f, VPad));
		S->SetHorizontalAlignment(HAlign_Fill);
	};

	// A cycling option is one wide button whose label carries the current value.
	WindowModeBtn = SkylandersUI::MakeButton(WidgetTree, TEXT("SetWindowMode"), TEXT("-"), 18);
	WindowModeBtn->OnClicked.AddDynamic(this, &USkylandersSettingsWidget::OnWindowModeClicked);
	AddRow(WindowModeBtn);

	ResolutionBtn = SkylandersUI::MakeButton(WidgetTree, TEXT("SetResolution"), TEXT("-"), 18);
	ResolutionBtn->OnClicked.AddDynamic(this, &USkylandersSettingsWidget::OnResolutionClicked);
	AddRow(ResolutionBtn);

	QualityBtn = SkylandersUI::MakeButton(WidgetTree, TEXT("SetQuality"), TEXT("-"), 18);
	QualityBtn->OnClicked.AddDynamic(this, &USkylandersSettingsWidget::OnQualityClicked);
	AddRow(QualityBtn);

	VSyncBtn = SkylandersUI::MakeButton(WidgetTree, TEXT("SetVSync"), TEXT("-"), 18);
	VSyncBtn->OnClicked.AddDynamic(this, &USkylandersSettingsWidget::OnVSyncClicked);
	AddRow(VSyncBtn);

	// Slider row: fixed-width label, stretching slider, fixed-width readout.
	auto MakeSliderRow = [&](FName BaseName, const FString& Label, float Min, float Max,
		USlider*& OutSlider, UTextBlock*& OutValue) -> UHorizontalBox*
	{
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), BaseName);

		UTextBlock* LabelText = SkylandersUI::MakeText(WidgetTree,
			FName(*(BaseName.ToString() + TEXT("_Lbl"))), Label, 18, SkylandersUI::White, ETextJustify::Left);
		USizeBox* LabelBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(),
			FName(*(BaseName.ToString() + TEXT("_LblBox"))));
		LabelBox->SetWidthOverride(230.f);
		LabelBox->AddChild(LabelText);
		UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(LabelBox);
		LabelSlot->SetVerticalAlignment(VAlign_Center);

		OutSlider = WidgetTree->ConstructWidget<USlider>(USlider::StaticClass(),
			FName(*(BaseName.ToString() + TEXT("_Slider"))));
		OutSlider->SetMinValue(Min);
		OutSlider->SetMaxValue(Max);
		OutSlider->SetStepSize(0.05f);
		UHorizontalBoxSlot* SliderSlot = Row->AddChildToHorizontalBox(OutSlider);
		SliderSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		SliderSlot->SetVerticalAlignment(VAlign_Center);
		SliderSlot->SetPadding(FMargin(8.f, 0.f));

		OutValue = SkylandersUI::MakeText(WidgetTree,
			FName(*(BaseName.ToString() + TEXT("_Val"))), TEXT("-"), 18, SkylandersUI::Gold, ETextJustify::Right);
		USizeBox* ValueBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(),
			FName(*(BaseName.ToString() + TEXT("_ValBox"))));
		ValueBox->SetWidthOverride(80.f);
		ValueBox->AddChild(OutValue);
		UHorizontalBoxSlot* ValueSlot = Row->AddChildToHorizontalBox(ValueBox);
		ValueSlot->SetVerticalAlignment(VAlign_Center);

		return Row;
	};

	AddRow(MakeSliderRow(TEXT("SetVolume"), TEXT("MASTER VOLUME"), 0.f, 1.f, VolumeSlider, VolumeValue), 10.f);
	VolumeSlider->OnValueChanged.AddDynamic(this, &USkylandersSettingsWidget::OnVolumeChanged);

	AddRow(MakeSliderRow(TEXT("SetSensitivity"), TEXT("MOUSE SENSITIVITY"),
		USkylandersGameSettings::MinSensitivity, USkylandersGameSettings::MaxSensitivity,
		SensitivitySlider, SensitivityValue), 10.f);
	SensitivitySlider->OnValueChanged.AddDynamic(this, &USkylandersSettingsWidget::OnSensitivityChanged);

	StatusText = SkylandersUI::MakeText(WidgetTree, TEXT("SetStatus"), TEXT(""), 14, SkylandersUI::Dim);
	AddRow(StatusText, 8.f);

	// APPLY / BACK side by side
	UHorizontalBox* Actions = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("SetActions"));

	UButton* ApplyBtn = SkylandersUI::MakeButton(WidgetTree, TEXT("SetApply"), TEXT("APPLY"), 20);
	ApplyBtn->OnClicked.AddDynamic(this, &USkylandersSettingsWidget::OnApplyClicked);
	UHorizontalBoxSlot* ApplySlot = Actions->AddChildToHorizontalBox(ApplyBtn);
	ApplySlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	ApplySlot->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));

	UButton* BackBtn = SkylandersUI::MakeButton(WidgetTree, TEXT("SetBack"), TEXT("BACK"), 20);
	BackBtn->OnClicked.AddDynamic(this, &USkylandersSettingsWidget::OnBackClicked);
	UHorizontalBoxSlot* BackSlot = Actions->AddChildToHorizontalBox(BackBtn);
	BackSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	BackSlot->SetPadding(FMargin(6.f, 0.f, 0.f, 0.f));

	AddRow(Actions, 12.f);

	BuildResolutionList();
	CaptureVideoSnapshot();
	RefreshLabels();
}

void USkylandersSettingsWidget::CaptureVideoSnapshot()
{
	if (UGameUserSettings* GS = UGameUserSettings::GetGameUserSettings())
	{
		InitialResolution = GS->GetScreenResolution();
		InitialWindowMode = static_cast<int32>(GS->GetFullscreenMode());
		bInitialVSync = GS->IsVSyncEnabled();
		InitialQuality = GS->ScalabilityQuality;
	}
}

void USkylandersSettingsWidget::RestoreVideoSnapshot()
{
	if (UGameUserSettings* GS = UGameUserSettings::GetGameUserSettings())
	{
		// Nothing was applied, so putting the staged values back is enough - no
		// ApplySettings call is needed to undo them.
		GS->SetScreenResolution(InitialResolution);
		GS->SetFullscreenMode(static_cast<EWindowMode::Type>(InitialWindowMode));
		GS->SetVSyncEnabled(bInitialVSync);
		GS->ScalabilityQuality = InitialQuality;
	}
}

void USkylandersSettingsWidget::BuildResolutionList()
{
	Resolutions.Reset();

	TArray<FIntPoint> Supported;
	if (UKismetSystemLibrary::GetSupportedFullscreenResolutions(Supported))
	{
		for (const FIntPoint& Res : Supported)
		{
			// Anything below 720p is not worth offering and clutters the cycle.
			if (Res.X >= 1280 && Res.Y >= 720)
			{
				Resolutions.AddUnique(Res);
			}
		}
	}

	if (Resolutions.Num() == 0)
	{
		// Headless or a display that reports nothing - fall back to common sizes.
		Resolutions.Add(FIntPoint(1280, 720));
		Resolutions.Add(FIntPoint(1600, 900));
		Resolutions.Add(FIntPoint(1920, 1080));
		Resolutions.Add(FIntPoint(2560, 1440));
	}

	// The current mode may be windowed and not in the supported-fullscreen list.
	if (UGameUserSettings* GS = UGameUserSettings::GetGameUserSettings())
	{
		Resolutions.AddUnique(GS->GetScreenResolution());
	}

	Resolutions.Sort([](const FIntPoint& A, const FIntPoint& B)
	{
		return A.X != B.X ? A.X < B.X : A.Y < B.Y;
	});
}

// ---------- Display ----------

void USkylandersSettingsWidget::RefreshLabels()
{
	UGameUserSettings* GS = UGameUserSettings::GetGameUserSettings();
	USkylandersGameSettings* Prefs = USkylandersGameSettings::Get();

	if (GS)
	{
		if (UTextBlock* T = SkylandersUI::GetButtonLabel(WindowModeBtn))
		{
			T->SetText(FText::FromString(FString::Printf(TEXT("WINDOW MODE:   %s"), *WindowModeName(GS->GetFullscreenMode()))));
		}

		if (UTextBlock* T = SkylandersUI::GetButtonLabel(ResolutionBtn))
		{
			const FIntPoint Res = GS->GetScreenResolution();
			T->SetText(FText::FromString(FString::Printf(TEXT("RESOLUTION:   %d x %d"), Res.X, Res.Y)));
		}

		if (UTextBlock* T = SkylandersUI::GetButtonLabel(QualityBtn))
		{
			T->SetText(FText::FromString(FString::Printf(TEXT("QUALITY:   %s"), *QualityName(GS->GetOverallScalabilityLevel()))));
		}

		if (UTextBlock* T = SkylandersUI::GetButtonLabel(VSyncBtn))
		{
			T->SetText(FText::FromString(FString::Printf(TEXT("V-SYNC:   %s"), GS->IsVSyncEnabled() ? TEXT("ON") : TEXT("OFF"))));
		}
	}

	if (Prefs)
	{
		if (VolumeSlider) VolumeSlider->SetValue(Prefs->MasterVolume);
		if (VolumeValue)
		{
			VolumeValue->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Prefs->MasterVolume * 100.f))));
		}

		if (SensitivitySlider) SensitivitySlider->SetValue(Prefs->MouseSensitivity);
		if (SensitivityValue)
		{
			SensitivityValue->SetText(FText::FromString(FString::Printf(TEXT("%.2f"), Prefs->MouseSensitivity)));
		}
	}

	if (StatusText)
	{
		StatusText->SetText(FText::FromString(bVideoDirty
			? TEXT("Press APPLY to use the new video settings. BACK discards them.")
			: TEXT("Volume and sensitivity apply immediately.")));
	}
}

// ---------- Video options (staged until APPLY) ----------

void USkylandersSettingsWidget::OnWindowModeClicked()
{
	if (UGameUserSettings* GS = UGameUserSettings::GetGameUserSettings())
	{
		GS->SetFullscreenMode(NextWindowMode(GS->GetFullscreenMode()));
		bVideoDirty = true;
	}
	RefreshLabels();
}

void USkylandersSettingsWidget::OnResolutionClicked()
{
	UGameUserSettings* GS = UGameUserSettings::GetGameUserSettings();
	if (GS && Resolutions.Num() > 0)
	{
		const int32 Current = Resolutions.IndexOfByKey(GS->GetScreenResolution());
		const int32 Next = (Current == INDEX_NONE) ? 0 : (Current + 1) % Resolutions.Num();
		GS->SetScreenResolution(Resolutions[Next]);
		bVideoDirty = true;
	}
	RefreshLabels();
}

void USkylandersSettingsWidget::OnQualityClicked()
{
	if (UGameUserSettings* GS = UGameUserSettings::GetGameUserSettings())
	{
		const int32 Current = GS->GetOverallScalabilityLevel();
		// -1 means the groups are mixed; start the cycle from Low in that case.
		const int32 Next = (Current < 0) ? 0 : (Current + 1) % 5;
		GS->SetOverallScalabilityLevel(Next);
		bVideoDirty = true;
	}
	RefreshLabels();
}

void USkylandersSettingsWidget::OnVSyncClicked()
{
	if (UGameUserSettings* GS = UGameUserSettings::GetGameUserSettings())
	{
		GS->SetVSyncEnabled(!GS->IsVSyncEnabled());
		bVideoDirty = true;
	}
	RefreshLabels();
}

void USkylandersSettingsWidget::OnApplyClicked()
{
	if (UGameUserSettings* GS = UGameUserSettings::GetGameUserSettings())
	{
		GS->ApplySettings(false);
		GS->SaveSettings();
	}
	bVideoDirty = false;

	// These are the values BACK should return to from now on.
	CaptureVideoSnapshot();
	RefreshLabels();
}

// ---------- Live options ----------

void USkylandersSettingsWidget::OnVolumeChanged(float NewValue)
{
	if (USkylandersGameSettings* Prefs = USkylandersGameSettings::Get())
	{
		Prefs->MasterVolume = NewValue;
		Prefs->ApplyAll();
	}

	if (VolumeValue)
	{
		VolumeValue->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(NewValue * 100.f))));
	}
}

void USkylandersSettingsWidget::OnSensitivityChanged(float NewValue)
{
	if (USkylandersGameSettings* Prefs = USkylandersGameSettings::Get())
	{
		Prefs->MouseSensitivity = NewValue;
		Prefs->ApplyAll();
	}

	if (SensitivityValue)
	{
		SensitivityValue->SetText(FText::FromString(FString::Printf(TEXT("%.2f"), NewValue)));
	}
}

// ---------- Close ----------

void USkylandersSettingsWidget::OnBackClicked()
{
	Close();
}

FReply USkylandersSettingsWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		Close();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void USkylandersSettingsWidget::Close()
{
	// Video changes that were never applied are dropped, so the screen never
	// leaves UGameUserSettings holding values the player did not confirm.
	if (bVideoDirty)
	{
		RestoreVideoSnapshot();
		bVideoDirty = false;
	}

	if (USkylandersGameSettings* Prefs = USkylandersGameSettings::Get())
	{
		Prefs->SaveAndApply();
	}

	RemoveFromParent();
	OnClosed.ExecuteIfBound();
}
