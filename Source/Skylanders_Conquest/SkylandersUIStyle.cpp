// Skylanders Conquest - Shared menu styling helpers implementation

#include "SkylandersUIStyle.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Blueprint/WidgetTree.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"

namespace SkylandersUI
{
	const FLinearColor Gold(0.95f, 0.78f, 0.20f, 1.0f);
	const FLinearColor White(0.92f, 0.92f, 0.95f, 1.0f);
	const FLinearColor Dim(0.62f, 0.64f, 0.72f, 1.0f);

	const FLinearColor ScreenBG(0.015f, 0.02f, 0.05f, 1.0f);
	const FLinearColor OverlayBG(0.010f, 0.015f, 0.04f, 0.92f);
	const FLinearColor PanelBG(0.03f, 0.04f, 0.09f, 0.96f);

	static const FLinearColor BtnNormal(0.08f, 0.10f, 0.18f, 0.95f);
	static const FLinearColor BtnHover(0.18f, 0.24f, 0.42f, 1.0f);
	static const FLinearColor BtnPress(0.30f, 0.38f, 0.60f, 1.0f);
	static const FLinearColor BtnOutline(0.35f, 0.42f, 0.65f, 0.8f);

	UTextBlock* MakeText(UWidgetTree* Tree, FName Name, const FString& Str, int32 FontSize,
		FLinearColor Color, ETextJustify::Type Justify)
	{
		if (!Tree) return nullptr;

		UTextBlock* TB = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		TB->SetText(FText::FromString(Str));

		FSlateFontInfo Font = TB->GetFont();
		Font.Size = FontSize;
		TB->SetFont(Font);

		TB->SetColorAndOpacity(FSlateColor(Color));
		TB->SetJustification(Justify);
		return TB;
	}

	UButton* MakeButton(UWidgetTree* Tree, FName Name, const FString& Label, int32 FontSize)
	{
		if (!Tree) return nullptr;

		UButton* Btn = Tree->ConstructWidget<UButton>(UButton::StaticClass(), Name);

		// RoundedBox renders a solid tinted rect without needing a texture resource,
		// which plain Box does not.
		auto SetBrush = [](FSlateBrush& Brush, FLinearColor Color)
		{
			Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
			Brush.TintColor = FSlateColor(Color);
			Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
			Brush.OutlineSettings.CornerRadii = FVector4(6.0f, 6.0f, 6.0f, 6.0f);
			Brush.OutlineSettings.Color = FSlateColor(BtnOutline);
			Brush.OutlineSettings.Width = 1.0f;
		};

		FButtonStyle Style = Btn->GetStyle();
		SetBrush(Style.Normal, BtnNormal);
		SetBrush(Style.Hovered, BtnHover);
		SetBrush(Style.Pressed, BtnPress);
		SetBrush(Style.Disabled, FLinearColor(0.06f, 0.07f, 0.10f, 0.7f));
		Btn->SetStyle(Style);

		// Qualified: other widget .cpp files declare their own file-static
		// MakeText, which lands in the same translation unit under unity builds.
		UTextBlock* TB = SkylandersUI::MakeText(Tree, FName(*(Name.ToString() + TEXT("_Txt"))), Label, FontSize, White);
		Btn->AddChild(TB);
		return Btn;
	}

	UTextBlock* GetButtonLabel(UButton* Btn)
	{
		return Btn ? Cast<UTextBlock>(Btn->GetChildAt(0)) : nullptr;
	}
}
