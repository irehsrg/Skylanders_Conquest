// Skylanders Conquest - Main gameplay HUD implementation

#include "SkylandersMainHUDWidget.h"
#include "SkylandersUIStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateTypes.h"

// Palette local to the HUD. Gold (SkylandersUI::Gold) stays the only accent;
// everything else is a shade of the panel navy or a bar color.
static const FLinearColor MainHUDPanelOutline(0.30f, 0.34f, 0.50f, 0.55f);
static const FLinearColor MainHUDTrack(0.01f, 0.012f, 0.025f, 0.95f);
static const FLinearColor MainHUDSlotBG(0.06f, 0.07f, 0.12f, 1.0f);
static const FLinearColor MainHUDHealthFill(0.80f, 0.20f, 0.10f, 1.0f);
static const FLinearColor MainHUDHealthGlow(1.00f, 0.55f, 0.22f, 0.45f);
static const FLinearColor MainHUDManaFill(0.13f, 0.38f, 0.92f, 1.0f);
static const FLinearColor MainHUDManaGlow(0.45f, 0.72f, 1.00f, 0.40f);
static const FLinearColor MainHUDPipEmpty(0.16f, 0.18f, 0.26f, 1.0f);
static const FLinearColor MainHUDCooldownShade(0.0f, 0.0f, 0.02f, 0.72f);

// Width of the HP/mana/XP column; the bars are the widest element of the cluster
static constexpr float MainHUDBarWidth = 340.0f;
static constexpr float MainHUDAbilitySize = 64.0f;
static constexpr float MainHUDPortraitSize = 96.0f;
static constexpr int32 MainHUDItemNameChars = 10;

// File-local names are prefixed: under unity builds other widget .cpp files
// with their own static helpers land in the same translation unit.
static UTextBlock* MainHUDText(UWidgetTree* Tree, const FString& Str, int32 Size, FLinearColor Color,
	bool bBold, bool bOutline)
{
	UTextBlock* TB = SkylandersUI::MakeText(Tree, NAME_None, Str, Size, Color);
	FSlateFontInfo Font = TB->GetFont();
	// The default UMG font is Roboto, whose composite font carries a "Bold" face
	if (bBold) Font.TypefaceFontName = FName(TEXT("Bold"));
	if (bOutline)
	{
		// 1px dark outline keeps numbers legible over bright bar fills and icons
		Font.OutlineSettings.OutlineSize = 1;
		Font.OutlineSettings.OutlineColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.9f);
	}
	TB->SetFont(Font);
	return TB;
}

static UBorder* MainHUDBox(UWidgetTree* Tree, const FSlateBrush& Brush, FMargin Padding)
{
	UBorder* B = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
	B->SetBrush(Brush);
	B->SetPadding(Padding);
	B->SetHorizontalAlignment(HAlign_Center);
	B->SetVerticalAlignment(VAlign_Center);
	return B;
}

static USizeBox* MainHUDSized(UWidgetTree* Tree, float Width, float Height, UWidget* Content)
{
	USizeBox* Box = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	if (Width > 0.0f) Box->SetWidthOverride(Width);
	if (Height > 0.0f) Box->SetHeightOverride(Height);
	if (Content) Box->AddChild(Content);
	return Box;
}

static UOverlaySlot* MainHUDOverlayAdd(UOverlay* Overlay, UWidget* Child, EHorizontalAlignment H, EVerticalAlignment V,
	FMargin Padding = FMargin(0.0f))
{
	UOverlaySlot* OSlot = Overlay->AddChildToOverlay(Child);
	OSlot->SetHorizontalAlignment(H);
	OSlot->SetVerticalAlignment(V);
	OSlot->SetPadding(Padding);
	return OSlot;
}

static FString MainHUDAbbreviate(const FString& Name)
{
	if (Name.Len() <= MainHUDItemNameChars) return Name;
	return Name.Left(MainHUDItemNameChars - 1).TrimEnd() + TEXT(".");
}

void USkylandersMainHUDWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (!WidgetTree) return;

	// Purely informational: never eat clicks meant for the shop or the game
	SetVisibility(ESlateVisibility::HitTestInvisible);

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("MainHUDRoot"));
	WidgetTree->RootWidget = Root;

	// ===== Bottom-center cluster =====
	UVerticalBox* Cluster = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("HUDCluster"));
	UCanvasPanelSlot* ClusterSlot = Root->AddChildToCanvas(Cluster);
	ClusterSlot->SetAnchors(FAnchors(0.5f, 1.0f, 0.5f, 1.0f));
	ClusterSlot->SetAlignment(FVector2D(0.5f, 1.0f));
	ClusterSlot->SetPosition(FVector2D(0.0f, -12.0f));
	ClusterSlot->SetAutoSize(true);

	UBorder* Panel = MainHUDBox(WidgetTree, SkylandersUI::RoundedBrush(SkylandersUI::PanelBG, 12.0f, MainHUDPanelOutline, 1.0f),
		FMargin(12.0f, 10.0f));
	UVerticalBoxSlot* PanelSlot = Cluster->AddChildToVerticalBox(Panel);
	PanelSlot->SetHorizontalAlignment(HAlign_Center);

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("HUDRow"));
	Panel->AddChild(Row);

	// -- Portrait with gold frame and a level badge hanging off its bottom-right corner
	{
		UOverlay* PortraitStack = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("PortraitStack"));

		PortraitImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Portrait"));
		FSlateBrush PortraitBrush = SkylandersUI::RoundedBrush(FLinearColor::White, 8.0f);
		PortraitBrush.ImageSize = FVector2D(MainHUDPortraitSize, MainHUDPortraitSize);
		PortraitImage->SetBrush(PortraitBrush);
		// No texture yet: a plain RoundedBox would draw solid white, so hold it dark
		PortraitImage->SetColorAndOpacity(MainHUDSlotBG);

		UBorder* Frame = MainHUDBox(WidgetTree, SkylandersUI::RoundedBrush(MainHUDTrack, 10.0f, SkylandersUI::Gold, 2.0f), FMargin(3.0f));
		Frame->AddChild(MainHUDSized(WidgetTree, MainHUDPortraitSize, MainHUDPortraitSize, PortraitImage));
		// Right/bottom padding leaves room for the badge to overlap the corner
		MainHUDOverlayAdd(PortraitStack, Frame, HAlign_Left, VAlign_Top, FMargin(0.0f, 0.0f, 8.0f, 8.0f));

		LevelText = MainHUDText(WidgetTree, TEXT("1"), 14, SkylandersUI::White, true, true);
		UBorder* Badge = MainHUDBox(WidgetTree, SkylandersUI::RoundedBrush(SkylandersUI::PanelBG, 14.0f, SkylandersUI::Gold, 2.0f), FMargin(0.0f));
		Badge->AddChild(LevelText);
		MainHUDOverlayAdd(PortraitStack, MainHUDSized(WidgetTree, 30.0f, 30.0f, Badge), HAlign_Right, VAlign_Bottom);

		UHorizontalBoxSlot* HSlot = Row->AddChildToHorizontalBox(PortraitStack);
		HSlot->SetVerticalAlignment(VAlign_Center);
		HSlot->SetPadding(FMargin(0.0f, 0.0f, 10.0f, 0.0f));
	}

	// -- Health / mana / XP column
	{
		UVerticalBox* Bars = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("VitalsColumn"));

		UVerticalBoxSlot* HPSlot = Bars->AddChildToVerticalBox(BuildVitalsBar(WidgetTree, 26.0f, MainHUDHealthFill, MainHUDHealthGlow, 15,
			HealthBar, HealthHighlight, HealthText));
		HPSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 5.0f));

		UVerticalBoxSlot* MPSlot = Bars->AddChildToVerticalBox(BuildVitalsBar(WidgetTree, 20.0f, MainHUDManaFill, MainHUDManaGlow, 13,
			ManaBar, ManaHighlight, ManaText));
		MPSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));

		XPBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("XPBar"));
		FProgressBarStyle XPStyle;
		XPStyle.SetBackgroundImage(SkylandersUI::RoundedBrush(MainHUDTrack, 3.0f));
		XPStyle.SetFillImage(SkylandersUI::RoundedBrush(FLinearColor::White, 3.0f));
		XPStyle.SetMarqueeImage(SkylandersUI::RoundedBrush(FLinearColor::White, 3.0f));
		XPBar->SetWidgetStyle(XPStyle);
		XPBar->SetFillColorAndOpacity(SkylandersUI::Gold);
		XPBar->SetPercent(0.0f);
		Bars->AddChildToVerticalBox(MainHUDSized(WidgetTree, 0.0f, 6.0f, XPBar));

		UHorizontalBoxSlot* HSlot = Row->AddChildToHorizontalBox(MainHUDSized(WidgetTree, MainHUDBarWidth, 0.0f, Bars));
		HSlot->SetVerticalAlignment(VAlign_Center);
		HSlot->SetPadding(FMargin(0.0f, 0.0f, 14.0f, 0.0f));
	}

	// -- Four ability slots
	AbilitySlotRoots.SetNum(NumAbilities);
	AbilityIcons.SetNum(NumAbilities);
	CooldownOverlays.SetNum(NumAbilities);
	CooldownTexts.SetNum(NumAbilities);
	RankPips.SetNum(NumAbilities * MaxRankPips);
	for (int32 i = 0; i < NumAbilities; i++)
	{
		UHorizontalBoxSlot* HSlot = Row->AddChildToHorizontalBox(BuildAbilitySlot(WidgetTree, i));
		HSlot->SetVerticalAlignment(VAlign_Center);
		HSlot->SetPadding(FMargin(i == 0 ? 0.0f : 4.0f, 0.0f, i == NumAbilities - 1 ? 0.0f : 4.0f, 0.0f));
		ApplyAbilitySlotLook(i);
	}

	// -- Six item cells under the panel
	{
		UHorizontalBox* Items = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ItemRow"));
		ItemCells.SetNum(NumItemSlots);
		ItemLabels.SetNum(NumItemSlots);
		for (int32 i = 0; i < NumItemSlots; i++)
		{
			UTextBlock* Label = MainHUDText(WidgetTree, TEXT(""), 11, SkylandersUI::White, true, false);
			Label->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
			UBorder* Cell = MainHUDBox(WidgetTree, SkylandersUI::RoundedBrush(SkylandersUI::PanelBG, 6.0f, MainHUDPanelOutline, 1.0f),
				FMargin(6.0f, 0.0f));
			Cell->AddChild(Label);

			UHorizontalBoxSlot* HSlot = Items->AddChildToHorizontalBox(MainHUDSized(WidgetTree, 100.0f, 26.0f, Cell));
			HSlot->SetPadding(FMargin(i == 0 ? 0.0f : 3.0f, 0.0f, i == NumItemSlots - 1 ? 0.0f : 3.0f, 0.0f));

			ItemCells[i] = Cell;
			ItemLabels[i] = Label;
		}
		UVerticalBoxSlot* ItemsSlot = Cluster->AddChildToVerticalBox(Items);
		ItemsSlot->SetHorizontalAlignment(HAlign_Center);
		ItemsSlot->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 0.0f));
		SetItems(TArray<FString>());
	}

	// ===== Top-right gold counter (the minimap sits right below it at y=60) =====
	{
		UHorizontalBox* GoldRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("GoldRow"));

		// Coin: a gold disc with a dark "G"; a glyph like U+25C6 is not in Roboto
		// and would fall back to a box on some setups
		UBorder* Coin = MainHUDBox(WidgetTree, SkylandersUI::RoundedBrush(SkylandersUI::Gold, 10.0f), FMargin(0.0f));
		Coin->AddChild(MainHUDText(WidgetTree, TEXT("G"), 11, SkylandersUI::PanelBG, true, false));
		UHorizontalBoxSlot* CoinSlot = GoldRow->AddChildToHorizontalBox(MainHUDSized(WidgetTree, 20.0f, 20.0f, Coin));
		CoinSlot->SetVerticalAlignment(VAlign_Center);
		CoinSlot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));

		GoldText = MainHUDText(WidgetTree, TEXT("0"), 18, SkylandersUI::Gold, true, true);
		GoldText->SetMinDesiredWidth(56.0f);
		GoldText->SetJustification(ETextJustify::Right);
		UHorizontalBoxSlot* TextSlot = GoldRow->AddChildToHorizontalBox(GoldText);
		TextSlot->SetVerticalAlignment(VAlign_Center);

		UBorder* GoldPanel = MainHUDBox(WidgetTree, SkylandersUI::RoundedBrush(SkylandersUI::PanelBG, 8.0f, MainHUDPanelOutline, 1.0f),
			FMargin(10.0f, 4.0f, 12.0f, 4.0f));
		GoldPanel->AddChild(GoldRow);

		UCanvasPanelSlot* GoldSlot = Root->AddChildToCanvas(GoldPanel);
		GoldSlot->SetAnchors(FAnchors(1.0f, 0.0f, 1.0f, 0.0f));
		GoldSlot->SetAlignment(FVector2D(1.0f, 0.0f));
		GoldSlot->SetPosition(FVector2D(-10.0f, 16.0f));
		GoldSlot->SetAutoSize(true);
	}
}

UWidget* USkylandersMainHUDWidget::BuildVitalsBar(UWidgetTree* Tree, float Height, FLinearColor Fill, FLinearColor Highlight,
	int32 FontSize, UProgressBar*& OutBar, UProgressBar*& OutHighlight, UTextBlock*& OutText)
{
	UOverlay* Stack = Tree->ConstructWidget<UOverlay>(UOverlay::StaticClass());

	// Base fill on a dark track
	OutBar = Tree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
	FProgressBarStyle Style;
	Style.SetBackgroundImage(SkylandersUI::RoundedBrush(MainHUDTrack, 5.0f, MainHUDPanelOutline, 1.0f));
	Style.SetFillImage(SkylandersUI::RoundedBrush(FLinearColor::White, 5.0f));
	Style.SetMarqueeImage(SkylandersUI::RoundedBrush(FLinearColor::White, 5.0f));
	OutBar->SetWidgetStyle(Style);
	OutBar->SetFillColorAndOpacity(Fill);
	OutBar->SetPercent(1.0f);
	MainHUDOverlayAdd(Stack, OutBar, HAlign_Fill, VAlign_Fill);

	// A lighter translucent fill over the top ~45% of the bar, driven to the
	// same percent: reads as a vertical gradient without a texture asset.
	OutHighlight = Tree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
	FProgressBarStyle GlowStyle;
	FSlateBrush NoTrack;
	NoTrack.DrawAs = ESlateBrushDrawType::NoDrawType;
	GlowStyle.SetBackgroundImage(NoTrack);
	GlowStyle.SetFillImage(SkylandersUI::RoundedBrush(FLinearColor::White, 4.0f));
	GlowStyle.SetMarqueeImage(NoTrack);
	OutHighlight->SetWidgetStyle(GlowStyle);
	OutHighlight->SetFillColorAndOpacity(Highlight);
	OutHighlight->SetPercent(1.0f);
	MainHUDOverlayAdd(Stack, MainHUDSized(Tree, 0.0f, FMath::RoundToFloat(Height * 0.45f), OutHighlight),
		HAlign_Fill, VAlign_Top, FMargin(1.0f, 1.0f, 1.0f, 0.0f));

	OutText = MainHUDText(Tree, TEXT("0 / 0"), FontSize, SkylandersUI::White, true, true);
	MainHUDOverlayAdd(Stack, OutText, HAlign_Center, VAlign_Center);

	return MainHUDSized(Tree, 0.0f, Height, Stack);
}

UWidget* USkylandersMainHUDWidget::BuildAbilitySlot(UWidgetTree* Tree, int32 Index)
{
	UVerticalBox* Column = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());

	UOverlay* Stack = Tree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), *FString::Printf(TEXT("AbilitySlot%d"), Index + 1));

	// Icon inside a framed rounded cell
	UImage* Icon = Tree->ConstructWidget<UImage>(UImage::StaticClass(), *FString::Printf(TEXT("AbilityIcon%d"), Index + 1));
	FSlateBrush IconBrush = SkylandersUI::RoundedBrush(FLinearColor::White, 6.0f);
	IconBrush.ImageSize = FVector2D(MainHUDAbilitySize, MainHUDAbilitySize);
	Icon->SetBrush(IconBrush);
	UBorder* Frame = MainHUDBox(Tree, SkylandersUI::RoundedBrush(MainHUDSlotBG, 8.0f, MainHUDPanelOutline, 1.0f), FMargin(2.0f));
	Frame->SetHorizontalAlignment(HAlign_Fill);
	Frame->SetVerticalAlignment(VAlign_Fill);
	Frame->AddChild(Icon);
	MainHUDOverlayAdd(Stack, Frame, HAlign_Fill, VAlign_Fill);

	// Cooldown shade + countdown, hidden while ready
	UTextBlock* CDText = MainHUDText(Tree, TEXT(""), 24, SkylandersUI::White, true, true);
	UBorder* Shade = MainHUDBox(Tree, SkylandersUI::RoundedBrush(MainHUDCooldownShade, 8.0f), FMargin(0.0f));
	Shade->AddChild(CDText);
	Shade->SetVisibility(ESlateVisibility::Collapsed);
	MainHUDOverlayAdd(Stack, Shade, HAlign_Fill, VAlign_Fill);

	// Key badge in the bottom-right corner
	UBorder* KeyBadge = MainHUDBox(Tree, SkylandersUI::RoundedBrush(SkylandersUI::PanelBG, 4.0f, MainHUDPanelOutline, 1.0f), FMargin(5.0f, 0.0f));
	KeyBadge->AddChild(MainHUDText(Tree, FString::FromInt(Index + 1), 10, SkylandersUI::White, true, true));
	MainHUDOverlayAdd(Stack, KeyBadge, HAlign_Right, VAlign_Bottom, FMargin(0.0f, 0.0f, 2.0f, 2.0f));

	Column->AddChildToVerticalBox(MainHUDSized(Tree, MainHUDAbilitySize, MainHUDAbilitySize, Stack));

	// Rank pips under the slot
	UHorizontalBox* Pips = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	for (int32 p = 0; p < MaxRankPips; p++)
	{
		UBorder* Pip = MainHUDBox(Tree, SkylandersUI::RoundedBrush(MainHUDPipEmpty, 2.0f), FMargin(0.0f));
		UHorizontalBoxSlot* PSlot = Pips->AddChildToHorizontalBox(MainHUDSized(Tree, 9.0f, 4.0f, Pip));
		PSlot->SetPadding(FMargin(p == 0 ? 0.0f : 1.5f, 0.0f, p == MaxRankPips - 1 ? 0.0f : 1.5f, 0.0f));
		RankPips[Index * MaxRankPips + p] = Pip;
	}
	UVerticalBoxSlot* PipsSlot = Column->AddChildToVerticalBox(Pips);
	PipsSlot->SetHorizontalAlignment(HAlign_Center);
	PipsSlot->SetPadding(FMargin(0.0f, 5.0f, 0.0f, 0.0f));

	AbilitySlotRoots[Index] = Stack;
	AbilityIcons[Index] = Icon;
	CooldownOverlays[Index] = Shade;
	CooldownTexts[Index] = CDText;
	return Column;
}

void USkylandersMainHUDWidget::ApplyAbilitySlotLook(int32 Index)
{
	if (!AbilityIcons.IsValidIndex(Index) || !AbilityIcons[Index]) return;

	// An icon-less RoundedBox brush draws its tint as a solid block, so an empty
	// slot is tinted down to the cell color instead of showing white.
	const bool bLearned = LastRank[Index] > 0;
	FLinearColor Tint = bHasIcon[Index] ? FLinearColor::White : MainHUDSlotBG;
	if (!bLearned && bHasIcon[Index]) Tint = FLinearColor(0.38f, 0.38f, 0.42f, 1.0f);
	AbilityIcons[Index]->SetColorAndOpacity(Tint);

	if (AbilitySlotRoots[Index])
	{
		AbilitySlotRoots[Index]->SetRenderOpacity(bLearned ? 1.0f : 0.55f);
	}
}

void USkylandersMainHUDWidget::SetVitals(float HP, float MaxHP, float MP, float MaxMP, int32 Level, float XPFrac, int32 Gold)
{
	const int32 IHP = FMath::FloorToInt(HP);
	const int32 IMaxHP = FMath::FloorToInt(MaxHP);
	const int32 IMP = FMath::FloorToInt(MP);
	const int32 IMaxMP = FMath::FloorToInt(MaxMP);

	if (IHP != LastHP || IMaxHP != LastMaxHP)
	{
		LastHP = IHP;
		LastMaxHP = IMaxHP;
		const float Pct = (MaxHP > 0.0f) ? FMath::Clamp(HP / MaxHP, 0.0f, 1.0f) : 0.0f;
		if (HealthBar) HealthBar->SetPercent(Pct);
		if (HealthHighlight) HealthHighlight->SetPercent(Pct);
		if (HealthText) HealthText->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), IHP, IMaxHP)));
	}

	if (IMP != LastMP || IMaxMP != LastMaxMP)
	{
		LastMP = IMP;
		LastMaxMP = IMaxMP;
		const float Pct = (MaxMP > 0.0f) ? FMath::Clamp(MP / MaxMP, 0.0f, 1.0f) : 0.0f;
		if (ManaBar) ManaBar->SetPercent(Pct);
		if (ManaHighlight) ManaHighlight->SetPercent(Pct);
		if (ManaText) ManaText->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), IMP, IMaxMP)));
	}

	if (Level != LastLevel)
	{
		LastLevel = Level;
		if (LevelText) LevelText->SetText(FText::AsNumber(Level));
	}

	if (XPFrac != LastXPFrac)
	{
		LastXPFrac = XPFrac;
		if (XPBar) XPBar->SetPercent(FMath::Clamp(XPFrac, 0.0f, 1.0f));
	}

	if (Gold != LastGold)
	{
		LastGold = Gold;
		if (GoldText) GoldText->SetText(FText::FromString(FString::FromInt(Gold)));
	}
}

void USkylandersMainHUDWidget::SetCooldown(int32 Index, float Remaining, bool bReady, int32 Rank)
{
	if (Index < 0 || Index >= NumAbilities) return;

	Rank = FMath::Clamp(Rank, 0, MaxRankPips);
	if (Rank != LastRank[Index])
	{
		LastRank[Index] = Rank;
		for (int32 p = 0; p < MaxRankPips; p++)
		{
			if (UBorder* Pip = RankPips[Index * MaxRankPips + p])
			{
				Pip->SetBrush(SkylandersUI::RoundedBrush(p < Rank ? SkylandersUI::Gold : MainHUDPipEmpty, 2.0f));
			}
		}
		ApplyAbilitySlotLook(Index);
	}

	// Whole seconds shown on the slot; 0 = ready (overlay hidden)
	const int32 Seconds = (!bReady && Remaining > 0.0f) ? FMath::CeilToInt(Remaining) : 0;
	if (Seconds == LastCooldownSeconds[Index]) return;
	const bool bWasShown = LastCooldownSeconds[Index] > 0;
	LastCooldownSeconds[Index] = Seconds;

	UBorder* Shade = CooldownOverlays[Index];
	UTextBlock* Text = CooldownTexts[Index];
	if (Seconds > 0)
	{
		if (!bWasShown && Shade) Shade->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (Text) Text->SetText(FText::AsNumber(Seconds));
	}
	else if (Shade)
	{
		Shade->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void USkylandersMainHUDWidget::SetAbilityArt(const TArray<UTexture2D*>& Icons, UTexture2D* Portrait)
{
	for (int32 i = 0; i < NumAbilities; i++)
	{
		UTexture2D* Tex = Icons.IsValidIndex(i) ? Icons[i] : nullptr;
		if (!AbilityIcons[i]) continue;
		AbilityIcons[i]->SetBrushFromTexture(Tex, false);
		bHasIcon[i] = Tex != nullptr;
		ApplyAbilitySlotLook(i);
	}

	if (PortraitImage)
	{
		PortraitImage->SetBrushFromTexture(Portrait, false);
		PortraitImage->SetColorAndOpacity(Portrait ? FLinearColor::White : MainHUDSlotBG);
	}
}

void USkylandersMainHUDWidget::SetItems(const TArray<FString>& Names)
{
	if (Names == LastItems && LastItems.Num() > 0) return;
	LastItems = Names;

	for (int32 i = 0; i < NumItemSlots; i++)
	{
		UTextBlock* Label = ItemLabels.IsValidIndex(i) ? ItemLabels[i] : nullptr;
		UBorder* Cell = ItemCells.IsValidIndex(i) ? ItemCells[i] : nullptr;
		if (!Label || !Cell) continue;

		const FString Name = Names.IsValidIndex(i) ? Names[i] : FString();
		if (Name.IsEmpty())
		{
			Label->SetText(FText::GetEmpty());
			Cell->SetBrush(SkylandersUI::RoundedBrush(SkylandersUI::PanelBG, 6.0f, MainHUDPanelOutline, 1.0f));
		}
		else
		{
			Label->SetText(FText::FromString(MainHUDAbbreviate(Name)));
			Label->SetColorAndOpacity(FSlateColor(SkylandersUI::White));
			// Owned items get the gold edge so the filled slots read at a glance
			Cell->SetBrush(SkylandersUI::RoundedBrush(FLinearColor(0.08f, 0.08f, 0.12f, 0.96f), 6.0f,
				FLinearColor(SkylandersUI::Gold.R, SkylandersUI::Gold.G, SkylandersUI::Gold.B, 0.7f), 1.0f));
		}
	}
}
