// Skylanders Conquest - Inventory HUD Widget Implementation

#include "SkylandersInventoryWidget.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/WidgetTree.h"

void USkylandersInventoryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree) return;

	// Root canvas
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("InvRoot"));
	WidgetTree->RootWidget = Root;

	// Background - anchored bottom-right
	UBorder* BG = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("InvBG"));
	BG->SetBrushColor(FLinearColor(0.02f, 0.02f, 0.05f, 0.75f));
	BG->SetPadding(FMargin(6.0f));

	UCanvasPanelSlot* BGSlot = Root->AddChildToCanvas(BG);
	BGSlot->SetAnchors(FAnchors(1.0f, 1.0f, 1.0f, 1.0f));
	BGSlot->SetAlignment(FVector2D(1.0f, 1.0f));
	BGSlot->SetPosition(FVector2D(-10.0f, -10.0f));
	BGSlot->SetSize(FVector2D(190.0f, 105.0f));

	UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("InvVBox"));
	BG->AddChild(VBox);

	// Title
	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("InvTitle"));
	Title->SetText(FText::FromString(TEXT("ITEMS")));
	FSlateFontInfo TitleFont = Title->GetFont();
	TitleFont.Size = 10;
	Title->SetFont(TitleFont);
	Title->SetColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.7f, 1.0f)));
	UVerticalBoxSlot* TitleSlot = VBox->AddChildToVerticalBox(Title);
	TitleSlot->SetPadding(FMargin(0, 0, 0, 2));

	SlotTexts.SetNum(6);

	// 3 rows x 2 columns
	for (int32 Row = 0; Row < 3; Row++)
	{
		UHorizontalBox* HBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(),
			*FString::Printf(TEXT("InvRow_%d"), Row));
		UVerticalBoxSlot* RowSlot = VBox->AddChildToVerticalBox(HBox);
		RowSlot->SetPadding(FMargin(0, 0, 0, 2));

		for (int32 Col = 0; Col < 2; Col++)
		{
			int32 Idx = Row * 2 + Col;

			UBorder* SlotBG = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),
				*FString::Printf(TEXT("InvSlotBG_%d"), Idx));
			SlotBG->SetBrushColor(FLinearColor(0.08f, 0.08f, 0.12f, 1.0f));
			SlotBG->SetPadding(FMargin(4, 2));

			UHorizontalBoxSlot* HSlot = HBox->AddChildToHorizontalBox(SlotBG);
			HSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			HSlot->SetPadding(FMargin(0, 0, (Col == 0) ? 3.0f : 0.0f, 0));

			UTextBlock* SlotText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),
				*FString::Printf(TEXT("InvSlot%d"), Idx));
			SlotText->SetText(FText::FromString(TEXT("---")));
			FSlateFontInfo Font = SlotText->GetFont();
			Font.Size = 9;
			SlotText->SetFont(Font);
			SlotText->SetColorAndOpacity(FSlateColor(FLinearColor(0.3f, 0.3f, 0.3f)));
			SlotText->SetJustification(ETextJustify::Center);
			SlotBG->AddChild(SlotText);

			SlotTexts[Idx] = SlotText;
		}
	}
}
