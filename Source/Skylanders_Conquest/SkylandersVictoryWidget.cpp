// Skylanders Conquest - Victory/Defeat End Screen Widget Implementation

#include "SkylandersVictoryWidget.h"
#include "SkylandersCharacter.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Fonts/SlateFontInfo.h"

static UTextBlock* MakeVictoryText(UWidgetTree* Tree, const FName& Name, int32 FontSize, FLinearColor Color, const FString& Default)
{
	UTextBlock* TB = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
	TB->SetText(FText::FromString(Default));
	FSlateFontInfo Font = TB->GetFont();
	Font.Size = FontSize;
	TB->SetFont(Font);
	TB->SetColorAndOpacity(FSlateColor(Color));
	TB->SetJustification(ETextJustify::Center);
	return TB;
}

void USkylandersVictoryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree) return;

	// Root canvas - full screen
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("VictoryRoot"));
	WidgetTree->RootWidget = Root;

	// Full-screen semi-transparent dark overlay
	UBorder* Overlay = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("VictoryOverlay"));
	Overlay->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.75f));
	Overlay->SetPadding(FMargin(0));

	UCanvasPanelSlot* OverlaySlot = Root->AddChildToCanvas(Overlay);
	OverlaySlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
	OverlaySlot->SetPosition(FVector2D(0.0f, 0.0f));
	OverlaySlot->SetSize(FVector2D(0.0f, 0.0f));
	OverlaySlot->SetAutoSize(false);

	// Centered content box
	UBorder* ContentBG = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("VictoryContentBG"));
	ContentBG->SetBrushColor(FLinearColor(0.02f, 0.02f, 0.06f, 0.9f));
	ContentBG->SetPadding(FMargin(40.0f, 30.0f));

	UCanvasPanelSlot* ContentSlot = Root->AddChildToCanvas(ContentBG);
	ContentSlot->SetAnchors(FAnchors(0.5f, 0.4f, 0.5f, 0.4f));
	ContentSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	ContentSlot->SetPosition(FVector2D(0.0f, 0.0f));
	ContentSlot->SetSize(FVector2D(500.0f, 320.0f));
	ContentSlot->SetAutoSize(false);

	// Vertical layout inside content box
	UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("VictoryVBox"));
	ContentBG->AddChild(VBox);

	auto AddRow = [&](UTextBlock* TB, float BottomPad = 6.0f)
	{
		UVerticalBoxSlot* VBSlot = VBox->AddChildToVerticalBox(TB);
		VBSlot->SetPadding(FMargin(0, 0, 0, BottomPad));
		VBSlot->SetHorizontalAlignment(HAlign_Center);
	};

	// VICTORY / DEFEAT text (large, colored later by ShowResult)
	ResultText = MakeVictoryText(WidgetTree, TEXT("VResultText"), 42, FLinearColor::White, TEXT("---"));
	AddRow(ResultText, 20);

	// Divider
	UBorder* Divider = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("VDivider"));
	Divider->SetBrushColor(FLinearColor(0.4f, 0.4f, 0.6f, 0.5f));
	Divider->SetPadding(FMargin(0));
	UVerticalBoxSlot* DivVBSlot = VBox->AddChildToVerticalBox(Divider);
	DivVBSlot->SetPadding(FMargin(0, 0, 0, 16));

	// Game time
	TimeText = MakeVictoryText(WidgetTree, TEXT("VTimeText"), 16, FLinearColor(0.7f, 0.7f, 0.8f), TEXT("Time: 00:00"));
	AddRow(TimeText, 8);

	// KDA + CS
	KDAText = MakeVictoryText(WidgetTree, TEXT("VKDAText"), 16, FLinearColor(0.8f, 0.8f, 0.8f), TEXT("K: 0  D: 0  CS: 0"));
	AddRow(KDAText, 8);

	// Gold
	GoldText = MakeVictoryText(WidgetTree, TEXT("VGoldText"), 16, FLinearColor(1.0f, 0.85f, 0.2f), TEXT("Gold: 0"));
	AddRow(GoldText, 8);

	// Level
	LevelText = MakeVictoryText(WidgetTree, TEXT("VLevelText"), 16, FLinearColor(0.6f, 0.8f, 1.0f), TEXT("Level: 1"));
	AddRow(LevelText, 0);
}

void USkylandersVictoryWidget::ShowResult(bool bVictory, ASkylandersCharacter* Player)
{
	if (ResultText)
	{
		if (bVictory)
		{
			ResultText->SetText(FText::FromString(TEXT("VICTORY")));
			ResultText->SetColorAndOpacity(FSlateColor(FLinearColor(0.2f, 1.0f, 0.3f)));
		}
		else
		{
			ResultText->SetText(FText::FromString(TEXT("DEFEAT")));
			ResultText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.2f, 0.2f)));
		}
	}

	if (!Player) return;

	if (TimeText)
	{
		int32 Minutes = FMath::FloorToInt(Player->GameElapsedTime / 60.0f);
		int32 Seconds = FMath::FloorToInt(FMath::Fmod(Player->GameElapsedTime, 60.0f));
		TimeText->SetText(FText::FromString(FString::Printf(TEXT("Time: %02d:%02d"), Minutes, Seconds)));
	}

	if (KDAText)
	{
		KDAText->SetText(FText::FromString(FString::Printf(
			TEXT("K: %d   D: %d   CS: %d"), Player->Kills, Player->Deaths, Player->CreepScore)));
	}

	if (GoldText)
	{
		GoldText->SetText(FText::FromString(FString::Printf(TEXT("Gold: %d"), Player->Coins)));
	}

	if (LevelText)
	{
		LevelText->SetText(FText::FromString(FString::Printf(TEXT("Level: %d"), Player->PlayerLevel)));
	}
}
