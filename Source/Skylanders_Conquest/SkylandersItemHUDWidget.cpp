// Skylanders Conquest - Item HUD Widget Implementation

#include "SkylandersItemHUDWidget.h"
#include "SkylandersCharacter.h"
#include "SkylandersItemCatalog.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Kismet/GameplayStatics.h"
#include "Fonts/SlateFontInfo.h"

void USkylandersItemHUDWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree) return;

	// Root canvas
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ItemHUDRoot"));
	WidgetTree->RootWidget = Root;

	// Horizontal box for 6 slots
	UHorizontalBox* HBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ItemHBox"));

	// Anchor bottom-center, position above ability cooldown area
	UCanvasPanelSlot* HBoxCanvasSlot = Root->AddChildToCanvas(HBox);
	HBoxCanvasSlot->SetAnchors(FAnchors(0.5f, 1.0f, 0.5f, 1.0f));
	HBoxCanvasSlot->SetAlignment(FVector2D(0.5f, 1.0f));
	// Total width = 6 * 40 + 5 * 2 = 250, position above the ability bar area
	HBoxCanvasSlot->SetPosition(FVector2D(0.0f, -100.0f));
	HBoxCanvasSlot->SetAutoSize(true);

	SlotLabels.SetNum(6);
	SlotBorders.SetNum(6);

	for (int32 i = 0; i < 6; i++)
	{
		// Each slot is a border with text inside
		UBorder* ItemBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),
			*FString::Printf(TEXT("ItemBorder_%d"), i));
		ItemBorder->SetBrushColor(FLinearColor(0.06f, 0.06f, 0.1f, 0.85f));
		ItemBorder->SetPadding(FMargin(0.0f));
		ItemBorder->SetDesiredSizeScale(FVector2D(1.0f, 1.0f));

		UHorizontalBoxSlot* BorderHSlot = HBox->AddChildToHorizontalBox(ItemBorder);
		BorderHSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		BorderHSlot->SetHorizontalAlignment(HAlign_Center);
		BorderHSlot->SetVerticalAlignment(VAlign_Center);
		// 2px gap between slots (right padding on all but last)
		BorderHSlot->SetPadding(FMargin(0, 0, (i < 5) ? 2.0f : 0.0f, 0));

		// Abbreviation text
		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),
			*FString::Printf(TEXT("ItemLabel_%d"), i));
		Label->SetText(FText::FromString(TEXT("---")));
		Label->SetMinDesiredWidth(40.0f);
		FSlateFontInfo Font = Label->GetFont();
		Font.Size = 10;
		Label->SetFont(Font);
		Label->SetColorAndOpacity(FSlateColor(FLinearColor(0.3f, 0.3f, 0.35f)));
		Label->SetJustification(ETextJustify::Center);
		ItemBorder->AddChild(Label);

		SlotLabels[i] = Label;
		SlotBorders[i] = ItemBorder;
	}
}

void USkylandersItemHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!IsVisible()) return;

	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	ASkylandersCharacter* Player = Cast<ASkylandersCharacter>(PlayerPawn);
	if (!Player) return;

	for (int32 i = 0; i < 6; i++)
	{
		if (i >= SlotLabels.Num() || !SlotLabels[i]) continue;
		if (i >= SlotBorders.Num() || !SlotBorders[i]) continue;

		if (i < Player->Inventory.Num() && Player->Inventory[i] > 0)
		{
			const FSkylandersItemData* ItemData = USkylandersItemCatalog::FindItem(Player->Inventory[i]);
			if (ItemData && ItemData->ItemName.Len() > 0)
			{
				// Build abbreviation: first 3 letters, uppercase
				FString Abbrev;
				int32 LetterCount = 0;
				for (int32 c = 0; c < ItemData->ItemName.Len() && LetterCount < 3; c++)
				{
					TCHAR Ch = ItemData->ItemName[c];
					if (FChar::IsAlpha(Ch))
					{
						Abbrev.AppendChar(FChar::ToUpper(Ch));
						LetterCount++;
					}
				}
				if (Abbrev.IsEmpty()) Abbrev = TEXT("???");

				SlotLabels[i]->SetText(FText::FromString(Abbrev));
				SlotLabels[i]->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.85f, 0.3f)));
				SlotBorders[i]->SetBrushColor(FLinearColor(0.12f, 0.1f, 0.02f, 0.9f));
			}
		}
		else
		{
			// Empty slot
			SlotLabels[i]->SetText(FText::FromString(TEXT("---")));
			SlotLabels[i]->SetColorAndOpacity(FSlateColor(FLinearColor(0.3f, 0.3f, 0.35f)));
			SlotBorders[i]->SetBrushColor(FLinearColor(0.06f, 0.06f, 0.1f, 0.85f));
		}
	}
}
