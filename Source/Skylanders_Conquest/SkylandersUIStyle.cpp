// Skylanders Conquest - Shared menu styling helpers implementation

#include "SkylandersUIStyle.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/SizeBox.h"
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
	const FLinearColor PanelOutline(0.30f, 0.34f, 0.50f, 0.55f);
	const FLinearColor Red(1.0f, 0.30f, 0.26f, 1.0f);

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
		// An unnamed button gets an unnamed label: "None_Txt" would collide on the
		// second unnamed button in the same tree.
		const FName LabelName = Name.IsNone() ? NAME_None : FName(*(Name.ToString() + TEXT("_Txt")));
		UTextBlock* TB = SkylandersUI::MakeText(Tree, LabelName, Label, FontSize, White);
		Btn->AddChild(TB);
		return Btn;
	}

	UTextBlock* GetButtonLabel(UButton* Btn)
	{
		return Btn ? Cast<UTextBlock>(Btn->GetChildAt(0)) : nullptr;
	}

	FSlateBrush RoundedBrush(FLinearColor Fill, float Radius, FLinearColor Outline, float OutlineWidth)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(Fill);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Brush.OutlineSettings.CornerRadii = FVector4(Radius, Radius, Radius, Radius);
		Brush.OutlineSettings.Color = FSlateColor(Outline);
		Brush.OutlineSettings.Width = OutlineWidth;
		return Brush;
	}

	void SetFontStyle(UTextBlock* TB, bool bBold, bool bOutline, FLinearColor OutlineColor)
	{
		if (!TB) return;
		FSlateFontInfo Font = TB->GetFont();
		// The default UMG font is Roboto, whose composite font carries a "Bold" face
		Font.TypefaceFontName = bBold ? FName(TEXT("Bold")) : FName(TEXT("Regular"));
		Font.OutlineSettings.OutlineSize = bOutline ? 1 : 0;
		Font.OutlineSettings.OutlineColor = OutlineColor;
		TB->SetFont(Font);
	}

	UTextBlock* MakeTitle(UWidgetTree* Tree, FName Name, const FString& Str, int32 FontSize,
		FLinearColor Color, ETextJustify::Type Justify)
	{
		UTextBlock* TB = SkylandersUI::MakeText(Tree, Name, Str, FontSize, Color, Justify);
		SetFontStyle(TB, true, true);
		return TB;
	}

	UBorder* MakePanel(UWidgetTree* Tree, FName Name, FLinearColor Fill, float Radius,
		FLinearColor Outline, float OutlineWidth, FMargin Padding)
	{
		if (!Tree) return nullptr;
		UBorder* B = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), Name);
		B->SetBrush(RoundedBrush(Fill, Radius, Outline, OutlineWidth));
		B->SetPadding(Padding);
		return B;
	}

	UWidget* MakeCoin(UWidgetTree* Tree, float Size)
	{
		if (!Tree) return nullptr;
		UBorder* Disc = MakePanel(Tree, NAME_None, Gold, Size * 0.5f, FLinearColor::Transparent, 0.0f, FMargin(0.0f));
		Disc->SetHorizontalAlignment(HAlign_Center);
		Disc->SetVerticalAlignment(VAlign_Center);
		UTextBlock* G = SkylandersUI::MakeText(Tree, NAME_None, TEXT("G"), FMath::Max(7, FMath::RoundToInt(Size * 0.55f)), PanelBG);
		SetFontStyle(G, true, false);
		Disc->AddChild(G);

		USizeBox* Box = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Box->SetWidthOverride(Size);
		Box->SetHeightOverride(Size);
		Box->AddChild(Disc);
		return Box;
	}

	UWidget* MakeKeyBadge(UWidgetTree* Tree, const FString& Key, float Size, FLinearColor Outline)
	{
		if (!Tree) return nullptr;
		UBorder* Badge = MakePanel(Tree, NAME_None, PanelBG, 4.0f, Outline, 1.0f, FMargin(0.0f));
		Badge->SetHorizontalAlignment(HAlign_Center);
		Badge->SetVerticalAlignment(VAlign_Center);
		Badge->AddChild(MakeTitle(Tree, NAME_None, Key, FMath::Max(8, FMath::RoundToInt(Size * 0.5f)), White));

		USizeBox* Box = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Box->SetWidthOverride(Size);
		Box->SetHeightOverride(Size);
		Box->AddChild(Badge);
		return Box;
	}

	void SetButtonHighlight(UButton* Btn, bool bOn, FLinearColor Accent)
	{
		if (!Btn) return;
		FButtonStyle Style = Btn->GetStyle();
		for (FSlateBrush* Brush : { &Style.Normal, &Style.Hovered, &Style.Pressed })
		{
			Brush->OutlineSettings.Color = FSlateColor(bOn ? Accent : BtnOutline);
			Brush->OutlineSettings.Width = bOn ? 2.0f : 1.0f;
		}
		Btn->SetStyle(Style);
	}
}
