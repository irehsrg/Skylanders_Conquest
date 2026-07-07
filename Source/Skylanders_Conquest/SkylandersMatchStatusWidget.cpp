// Skylanders Conquest - Match Status Bar Implementation

#include "SkylandersMatchStatusWidget.h"
#include "SkylandersCharacter.h"
#include "SkylandersEnemyGod.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Blueprint/WidgetTree.h"
#include "Kismet/GameplayStatics.h"
#include "Fonts/SlateFontInfo.h"

static UTextBlock* MakeStatusText(UWidgetTree* Tree, int32 Size, FLinearColor Color)
{
	UTextBlock* TB = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	FSlateFontInfo Font = TB->GetFont();
	Font.Size = Size;
	TB->SetFont(Font);
	TB->SetColorAndOpacity(FSlateColor(Color));
	TB->SetJustification(ETextJustify::Center);
	return TB;
}

void USkylandersMatchStatusWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (!WidgetTree) return;

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("StatusRoot"));
	WidgetTree->RootWidget = Root;

	// Dark bar wrapping a horizontal box, anchored top-center
	UBorder* Bar = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("StatusBar"));
	Bar->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.55f));
	Bar->SetPadding(FMargin(16.0f, 4.0f));

	UCanvasPanelSlot* BarSlot = Root->AddChildToCanvas(Bar);
	BarSlot->SetAnchors(FAnchors(0.5f, 0.0f, 0.5f, 0.0f));
	BarSlot->SetAlignment(FVector2D(0.5f, 0.0f));
	BarSlot->SetPosition(FVector2D(0.0f, 8.0f));
	BarSlot->SetAutoSize(true);

	UHorizontalBox* HBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Bar->AddChild(HBox);

	auto AddCell = [&](UTextBlock* TB, float SidePad)
	{
		UHorizontalBoxSlot* HSlot = HBox->AddChildToHorizontalBox(TB);
		HSlot->SetPadding(FMargin(SidePad, 0.0f));
		HSlot->SetVerticalAlignment(VAlign_Center);
	};

	BlueGoldText = MakeStatusText(WidgetTree, 17, FLinearColor(0.35f, 0.6f, 1.0f));
	BlueGoldText->SetText(FText::FromString(TEXT("0")));
	AddCell(BlueGoldText, 10.0f);

	TimerText = MakeStatusText(WidgetTree, 20, FLinearColor::White);
	TimerText->SetText(FText::FromString(TEXT("00:00")));
	AddCell(TimerText, 16.0f);

	RedGoldText = MakeStatusText(WidgetTree, 17, FLinearColor(1.0f, 0.4f, 0.35f));
	RedGoldText->SetText(FText::FromString(TEXT("0")));
	AddCell(RedGoldText, 10.0f);
}

void USkylandersMatchStatusWidget::NativeTick(const FGeometry& MyGeometry, float DeltaTime)
{
	Super::NativeTick(MyGeometry, DeltaTime);

	ASkylandersCharacter* Player = Cast<ASkylandersCharacter>(GetOwningPlayerPawn());
	int32 BlueGold = 0;
	if (Player)
	{
		if (TimerText)
		{
			int32 Minutes = FMath::FloorToInt(Player->GameElapsedTime / 60.0f);
			int32 Seconds = FMath::FloorToInt(FMath::Fmod(Player->GameElapsedTime, 60.0f));
			TimerText->SetText(FText::FromString(FString::Printf(TEXT("%02d:%02d"), Minutes, Seconds)));
		}
		BlueGold += Player->TotalGoldEarned;
	}

	// Team economy = player + ally gods (blue) vs enemy gods (red)
	if (GetWorld())
	{
		int32 RedGold = 0;
		TArray<AActor*> Gods;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASkylandersEnemyGod::StaticClass(), Gods);
		for (AActor* A : Gods)
		{
			if (ASkylandersEnemyGod* God = Cast<ASkylandersEnemyGod>(A))
			{
				if (God->Team == ETowerTeam::Enemy) RedGold += God->Gold;
				else BlueGold += God->Gold;
			}
		}
		if (RedGoldText) RedGoldText->SetText(FText::FromString(FString::Printf(TEXT("%d"), RedGold)));
	}

	if (BlueGoldText) BlueGoldText->SetText(FText::FromString(FString::Printf(TEXT("%d"), BlueGold)));
}
