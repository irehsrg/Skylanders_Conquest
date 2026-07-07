// Skylanders Conquest - Kill Feed Implementation

#include "SkylandersKillFeedWidget.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Fonts/SlateFontInfo.h"
#include "TimerManager.h"
#include "Engine/World.h"

void USkylandersKillFeedWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (!WidgetTree) return;

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("KillFeedRoot"));
	WidgetTree->RootWidget = Root;

	FeedBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("KillFeedBox"));
	UCanvasPanelSlot* CanvasSlot = Root->AddChildToCanvas(FeedBox);
	// Anchor top-right, sit just under the minimap
	CanvasSlot->SetAnchors(FAnchors(1.0f, 0.0f, 1.0f, 0.0f));
	CanvasSlot->SetAlignment(FVector2D(1.0f, 0.0f));
	CanvasSlot->SetPosition(FVector2D(-30.0f, 230.0f));
	CanvasSlot->SetAutoSize(true);
}

void USkylandersKillFeedWidget::AddEntry(const FString& Text, FLinearColor Color)
{
	if (!FeedBox || !WidgetTree) return;

	// Row: dark rounded-ish border wrapping the text
	UBorder* Row = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Row->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.55f));
	Row->SetPadding(FMargin(10.0f, 4.0f));

	UTextBlock* TB = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	TB->SetText(FText::FromString(Text));
	FSlateFontInfo Font = TB->GetFont();
	Font.Size = 15;
	TB->SetFont(Font);
	TB->SetColorAndOpacity(FSlateColor(Color));
	TB->SetJustification(ETextJustify::Right);
	Row->AddChild(TB);

	// Newest on top
	UVerticalBoxSlot* VBSlot = FeedBox->AddChildToVerticalBox(Row);
	VBSlot->SetPadding(FMargin(0, 0, 0, 4));
	VBSlot->SetHorizontalAlignment(HAlign_Right);
	FeedBox->ShiftChild(0, Row);

	// Cap the number of visible rows
	while (FeedBox->GetChildrenCount() > MaxEntries)
	{
		FeedBox->RemoveChildAt(FeedBox->GetChildrenCount() - 1);
	}

	// Auto-remove this row after a few seconds
	if (UWorld* World = GetWorld())
	{
		FTimerHandle Handle;
		TWeakObjectPtr<UBorder> WeakRow(Row);
		TWeakObjectPtr<UVerticalBox> WeakBox(FeedBox);
		World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [WeakRow, WeakBox]()
		{
			if (WeakRow.IsValid() && WeakBox.IsValid())
			{
				WeakBox->RemoveChild(WeakRow.Get());
			}
		}), 5.0f, false);
	}
}

void USkylandersKillFeedWidget::Post(UObject* WorldContext, const FString& Text, FLinearColor Color)
{
	if (!WorldContext) return;
	TArray<UUserWidget*> Found;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(WorldContext, Found, USkylandersKillFeedWidget::StaticClass(), false);
	for (UUserWidget* W : Found)
	{
		if (USkylandersKillFeedWidget* Feed = Cast<USkylandersKillFeedWidget>(W))
		{
			Feed->AddEntry(Text, Color);
			return;
		}
	}
}
