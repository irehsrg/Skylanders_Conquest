// Skylanders Conquest - Announcer banner implementation

#include "SkylandersAnnouncerWidget.h"
#include "SkylandersSoundLibrary.h"
#include "SkylandersSmokeTest.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Fonts/SlateFontInfo.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

void USkylandersAnnouncerWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (!WidgetTree) return;

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("AnnouncerRoot"));
	WidgetTree->RootWidget = Root;

	BannerText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("AnnouncerText"));
	FSlateFontInfo Font = BannerText->GetFont();
	Font.Size = 44;
	// Thick dark outline so the banner reads over any terrain or VFX
	Font.OutlineSettings.OutlineSize = 3;
	Font.OutlineSettings.OutlineColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.9f);
	BannerText->SetFont(Font);
	BannerText->SetJustification(ETextJustify::Center);
	BannerText->SetVisibility(ESlateVisibility::Collapsed);

	UCanvasPanelSlot* CanvasSlot = Root->AddChildToCanvas(BannerText);
	CanvasSlot->SetAnchors(FAnchors(0.5f, 0.22f, 0.5f, 0.22f));
	CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	CanvasSlot->SetPosition(FVector2D::ZeroVector);
	CanvasSlot->SetAutoSize(true);

	// Presentation only: never eat clicks meant for the shop or pause menu
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void USkylandersAnnouncerWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(StepTimer);
	}
	Super::NativeDestruct();
}

void USkylandersAnnouncerWidget::Enqueue(FEntry&& Entry)
{
	// Keep the newest news: when the backlog is full, the oldest pending banner
	// is the least relevant one.
	if (Pending.Num() >= MaxPending)
	{
		Pending.RemoveAt(0);
	}
	Pending.Add(MoveTemp(Entry));
	if (!bShowing)
	{
		ShowNext();
	}
}

void USkylandersAnnouncerWidget::ShowNext()
{
	UWorld* World = GetWorld();
	if (!BannerText || !World || Pending.Num() == 0)
	{
		bShowing = false;
		if (BannerText) BannerText->SetVisibility(ESlateVisibility::Collapsed);
		if (World) World->GetTimerManager().ClearTimer(StepTimer);
		return;
	}

	FEntry Entry = Pending[0];
	Pending.RemoveAt(0);

	bShowing = true;
	ShownTime = 0.0f;
	HoldSeconds = FMath::Max(Entry.Seconds, FadeInSeconds);
	ShownColor = Entry.Color;
	BannerText->SetText(FText::FromString(Entry.Text));
	BannerText->SetColorAndOpacity(FSlateColor(FLinearColor(ShownColor.R, ShownColor.G, ShownColor.B, 0.0f)));
	BannerText->SetVisibility(ESlateVisibility::HitTestInvisible);
	SKY_COUNT("announce");

	if (!Entry.SoundName.IsEmpty())
	{
		SkylandersSound::Play2D(this, *Entry.SoundName);
	}

	// Driven by a world timer rather than NativeTick: Slate stops ticking a
	// widget whose content is collapsed, and the banner has to wake itself up.
	if (!World->GetTimerManager().IsTimerActive(StepTimer))
	{
		World->GetTimerManager().SetTimer(StepTimer, this, &USkylandersAnnouncerWidget::Step, StepSeconds, true);
	}
}

void USkylandersAnnouncerWidget::Step()
{
	if (!bShowing || !BannerText) return;

	ShownTime += StepSeconds;
	const float Alpha = FMath::Clamp(ShownTime / FadeInSeconds, 0.0f, 1.0f);
	BannerText->SetColorAndOpacity(FSlateColor(FLinearColor(ShownColor.R, ShownColor.G, ShownColor.B, Alpha)));

	if (ShownTime >= HoldSeconds)
	{
		BannerText->SetVisibility(ESlateVisibility::Collapsed);
		bShowing = false;
		ShowNext();
	}
}

void USkylandersAnnouncerWidget::Announce(UObject* WorldContext, const FString& Text, FLinearColor Color, float Seconds, const TCHAR* SoundName)
{
	if (!WorldContext) return;

	TArray<UUserWidget*> Found;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(WorldContext, Found, USkylandersAnnouncerWidget::StaticClass(), false);
	for (UUserWidget* W : Found)
	{
		if (USkylandersAnnouncerWidget* Announcer = Cast<USkylandersAnnouncerWidget>(W))
		{
			FEntry Entry;
			Entry.Text = Text;
			Entry.Color = Color;
			Entry.Seconds = Seconds;
			Entry.SoundName = SoundName ? SoundName : TEXT("");
			Announcer->Enqueue(MoveTemp(Entry));
			return;
		}
	}

	// No banner on screen: the cue still matters (kill confirms)
	if (SoundName)
	{
		SkylandersSound::Play2D(WorldContext, SoundName);
	}
}

bool USkylandersAnnouncerWidget::ClaimFirstBlood(const UObject* WorldContext)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World) return false;
	// Keyed by world so a new match (new UWorld) gets its own first blood
	static TWeakObjectPtr<UWorld> ClaimedWorld;
	if (ClaimedWorld.Get() == World) return false;
	ClaimedWorld = World;
	return true;
}
