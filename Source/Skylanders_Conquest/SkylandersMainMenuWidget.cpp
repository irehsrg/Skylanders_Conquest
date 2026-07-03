// Skylanders Conquest - Front-End Main Menu Widget Implementation

#include "SkylandersMainMenuWidget.h"
#include "SkylandersGameInstance.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Blueprint/WidgetTree.h"
#include "Fonts/SlateFontInfo.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "GameFramework/PlayerController.h"

// ---- Shared style constants ----
static const FLinearColor MenuGold(0.95f, 0.78f, 0.20f, 1.0f);
static const FLinearColor MenuWhite(0.92f, 0.92f, 0.95f, 1.0f);
static const FLinearColor MenuBtnNormal(0.08f, 0.10f, 0.18f, 0.95f);
static const FLinearColor MenuBtnHover(0.18f, 0.24f, 0.42f, 1.0f);
static const FLinearColor MenuBtnPress(0.30f, 0.38f, 0.60f, 1.0f);

static UTextBlock* MakeText(UWidgetTree* Tree, FName Name, const FString& Str, int32 Size, FLinearColor Color)
{
	UTextBlock* TB = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
	TB->SetText(FText::FromString(Str));
	FSlateFontInfo Font = TB->GetFont();
	Font.Size = Size;
	TB->SetFont(Font);
	TB->SetColorAndOpacity(FSlateColor(Color));
	TB->SetJustification(ETextJustify::Center);
	return TB;
}

UButton* USkylandersMainMenuWidget::MakeMenuButton(const FString& Label, FName Name)
{
	UButton* Btn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);

	FButtonStyle Style = Btn->GetStyle();
	auto SetBrush = [](FSlateBrush& Brush, FLinearColor Color)
	{
		Brush.TintColor = FSlateColor(Color);
		Brush.DrawAs = ESlateBrushDrawType::Box;
	};
	SetBrush(Style.Normal, MenuBtnNormal);
	SetBrush(Style.Hovered, MenuBtnHover);
	SetBrush(Style.Pressed, MenuBtnPress);
	Btn->SetStyle(Style);

	UTextBlock* TB = MakeText(WidgetTree, FName(*(Name.ToString() + TEXT("_Txt"))), Label, 22, MenuWhite);
	Btn->AddChild(TB);
	return Btn;
}

void USkylandersMainMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (!WidgetTree) return;

	// Root canvas
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("MenuRoot"));
	WidgetTree->RootWidget = Root;

	// Dark full-screen background
	UBorder* BG = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MenuBG"));
	BG->SetBrushColor(FLinearColor(0.015f, 0.02f, 0.05f, 1.0f));
	UCanvasPanelSlot* BGSlot = Root->AddChildToCanvas(BG);
	BGSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	BGSlot->SetOffsets(FMargin(0));

	// Title
	UTextBlock* Title = MakeText(WidgetTree, TEXT("MenuTitle"), TEXT("SKYLANDERS CONQUEST"), 52, MenuGold);
	UCanvasPanelSlot* TitleSlot = Root->AddChildToCanvas(Title);
	TitleSlot->SetAnchors(FAnchors(0.5f, 0.12f, 0.5f, 0.12f));
	TitleSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	TitleSlot->SetAutoSize(true);

	UTextBlock* Subtitle = MakeText(WidgetTree, TEXT("MenuSubtitle"), TEXT("A Skylanders MOBA"), 18, MenuWhite);
	UCanvasPanelSlot* SubSlot = Root->AddChildToCanvas(Subtitle);
	SubSlot->SetAnchors(FAnchors(0.5f, 0.18f, 0.5f, 0.18f));
	SubSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	SubSlot->SetAutoSize(true);

	// Screen switcher (centered)
	ScreenSwitcher = WidgetTree->ConstructWidget<UWidgetSwitcher>(UWidgetSwitcher::StaticClass(), TEXT("MenuSwitcher"));
	UCanvasPanelSlot* SwSlot = Root->AddChildToCanvas(ScreenSwitcher);
	SwSlot->SetAnchors(FAnchors(0.5f, 0.55f, 0.5f, 0.55f));
	SwSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	SwSlot->SetSize(FVector2D(420.f, 360.f));
	SwSlot->SetAutoSize(false);

	ScreenSwitcher->AddChild(BuildMainScreen());        // Screen_Main
	ScreenSwitcher->AddChild(BuildCharacterScreen());   // Screen_Characters
	ScreenSwitcher->AddChild(BuildSettingsScreen());    // Screen_Settings
	ScreenSwitcher->SetActiveWidgetIndex(Screen_Main);
}

UVerticalBox* USkylandersMainMenuWidget::BuildMainScreen()
{
	UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MainScreen"));

	auto AddBtn = [&](UButton* Btn)
	{
		UVerticalBoxSlot* S = Box->AddChildToVerticalBox(Btn);
		S->SetPadding(FMargin(0.f, 8.f));
		S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		S->SetHorizontalAlignment(HAlign_Fill);
	};

	UButton* PlayBtn = MakeMenuButton(TEXT("PLAY"), TEXT("PlayBtn"));
	PlayBtn->OnClicked.AddDynamic(this, &USkylandersMainMenuWidget::OnPlayClicked);
	AddBtn(PlayBtn);

	UButton* CharBtn = MakeMenuButton(TEXT("CHARACTERS"), TEXT("CharBtn"));
	CharBtn->OnClicked.AddDynamic(this, &USkylandersMainMenuWidget::OnCharactersClicked);
	AddBtn(CharBtn);

	UButton* SetBtn = MakeMenuButton(TEXT("SETTINGS"), TEXT("SetBtn"));
	SetBtn->OnClicked.AddDynamic(this, &USkylandersMainMenuWidget::OnSettingsClicked);
	AddBtn(SetBtn);

	UButton* QuitBtn = MakeMenuButton(TEXT("QUIT"), TEXT("QuitBtn"));
	QuitBtn->OnClicked.AddDynamic(this, &USkylandersMainMenuWidget::OnQuitClicked);
	AddBtn(QuitBtn);

	return Box;
}

UButton* USkylandersMainMenuWidget::AddCharacterButton(UVerticalBox* Box, const FString& Name, const FString& Role,
	const FString& KitLine, FLinearColor AccentColor, FName WidgetName)
{
	UButton* Btn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), WidgetName);

	FButtonStyle Style = Btn->GetStyle();
	auto SetBrush = [](FSlateBrush& Brush, FLinearColor Color)
	{
		Brush.TintColor = FSlateColor(Color);
		Brush.DrawAs = ESlateBrushDrawType::Box;
	};
	SetBrush(Style.Normal, MenuBtnNormal);
	SetBrush(Style.Hovered, MenuBtnHover);
	SetBrush(Style.Pressed, MenuBtnPress);
	Btn->SetStyle(Style);

	// Name (accent color), role, and kit line stacked inside the button
	UVerticalBox* Inner = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(),
		FName(*(WidgetName.ToString() + TEXT("_Box"))));

	UTextBlock* NameText = MakeText(WidgetTree, FName(*(WidgetName.ToString() + TEXT("_Name"))), Name, 20, AccentColor);
	Inner->AddChildToVerticalBox(NameText)->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));

	UTextBlock* RoleText = MakeText(WidgetTree, FName(*(WidgetName.ToString() + TEXT("_Role"))), Role, 13, MenuWhite);
	Inner->AddChildToVerticalBox(RoleText);

	UTextBlock* KitText = MakeText(WidgetTree, FName(*(WidgetName.ToString() + TEXT("_Kit"))), KitLine, 10,
		FLinearColor(0.65f, 0.65f, 0.72f, 1.0f));
	Inner->AddChildToVerticalBox(KitText)->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));

	Btn->AddChild(Inner);
	Box->AddChildToVerticalBox(Btn)->SetPadding(FMargin(0.f, 5.f));
	return Btn;
}

UVerticalBox* USkylandersMainMenuWidget::BuildCharacterScreen()
{
	UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("CharScreen"));

	UTextBlock* Header = MakeText(WidgetTree, TEXT("CharHeader"), TEXT("SELECT YOUR SKYLANDER"), 24, MenuGold);
	Box->AddChildToVerticalBox(Header)->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));

	UButton* THBtn = AddCharacterButton(Box,
		TEXT("TRIGGER HAPPY"), TEXT("Gunslinger  -  Ranged Carry"),
		TEXT("Gatling Gun / Pot o' Gold / Golden Machine Gun / Yamato Cannon"),
		MenuGold, TEXT("CharTH"));
	THBtn->OnClicked.AddDynamic(this, &USkylandersMainMenuWidget::OnSelectTriggerHappy);

	UButton* HexBtn = AddCharacterButton(Box,
		TEXT("HEX"), TEXT("Undead Sorceress  -  Mage"),
		TEXT("Phantom Orb / Wall of Bones / Rain of Skulls / Skull Storm"),
		FLinearColor(0.70f, 0.35f, 1.0f, 1.0f), TEXT("CharHex"));
	HexBtn->OnClicked.AddDynamic(this, &USkylandersMainMenuWidget::OnSelectHex);

	UButton* TreeRexBtn = AddCharacterButton(Box,
		TEXT("TREE REX"), TEXT("Giant Guardian  -  Tank / Support"),
		TEXT("Shockwave Slam / Barkskin / Healing Grove / Titan's Wrath"),
		FLinearColor(0.35f, 0.85f, 0.30f, 1.0f), TEXT("CharTreeRex"));
	TreeRexBtn->OnClicked.AddDynamic(this, &USkylandersMainMenuWidget::OnSelectTreeRex);

	UButton* Back = MakeMenuButton(TEXT("BACK"), TEXT("CharBack"));
	Back->OnClicked.AddDynamic(this, &USkylandersMainMenuWidget::OnBackClicked);
	Box->AddChildToVerticalBox(Back)->SetPadding(FMargin(0.f, 14.f, 0.f, 0.f));

	return Box;
}

UVerticalBox* USkylandersMainMenuWidget::BuildSettingsScreen()
{
	UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SettingsScreen"));

	UTextBlock* Header = MakeText(WidgetTree, TEXT("SetHeader"), TEXT("SETTINGS"), 24, MenuGold);
	Box->AddChildToVerticalBox(Header)->SetPadding(FMargin(0.f, 0.f, 0.f, 16.f));

	UTextBlock* Note = MakeText(WidgetTree, TEXT("SetNote"),
		TEXT("Audio, video and controls\nsettings coming soon."), 16, MenuWhite);
	Box->AddChildToVerticalBox(Note)->SetPadding(FMargin(0.f, 0.f, 0.f, 16.f));

	UButton* Back = MakeMenuButton(TEXT("BACK"), TEXT("SetBack"));
	Back->OnClicked.AddDynamic(this, &USkylandersMainMenuWidget::OnBackClicked);
	Box->AddChildToVerticalBox(Back)->SetPadding(FMargin(0.f, 18.f, 0.f, 0.f));

	return Box;
}

// ---- Handlers ----
void USkylandersMainMenuWidget::OnPlayClicked()
{
	// FInputModeUIOnly flags the game viewport as ignore-input, and that flag
	// survives OpenLevel — restore game input before travel or the match loads
	// with dead controls.
	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->bShowMouseCursor = false;
	}
	UGameplayStatics::OpenLevel(this, GameLevelName);
}

void USkylandersMainMenuWidget::OnCharactersClicked()
{
	if (ScreenSwitcher) ScreenSwitcher->SetActiveWidgetIndex(Screen_Characters);
}

void USkylandersMainMenuWidget::OnSettingsClicked()
{
	if (ScreenSwitcher) ScreenSwitcher->SetActiveWidgetIndex(Screen_Settings);
}

void USkylandersMainMenuWidget::OnBackClicked()
{
	if (ScreenSwitcher) ScreenSwitcher->SetActiveWidgetIndex(Screen_Main);
}

void USkylandersMainMenuWidget::StartGameAs(FName CharacterID)
{
	// Remember the pick across level travel
	if (USkylandersGameInstance* GI = GetGameInstance<USkylandersGameInstance>())
	{
		GI->SelectedCharacterID = CharacterID;
	}

	// FInputModeUIOnly survives OpenLevel — restore game input before travel
	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->bShowMouseCursor = false;
	}
	UGameplayStatics::OpenLevel(this, GameLevelName);
}

void USkylandersMainMenuWidget::OnSelectTriggerHappy() { StartGameAs(TEXT("TriggerHappy")); }
void USkylandersMainMenuWidget::OnSelectHex()          { StartGameAs(TEXT("Hex")); }
void USkylandersMainMenuWidget::OnSelectTreeRex()      { StartGameAs(TEXT("TreeRex")); }

void USkylandersMainMenuWidget::OnQuitClicked()
{
	APlayerController* PC = GetOwningPlayer();
	UKismetSystemLibrary::QuitGame(this, PC, EQuitPreference::Quit, false);
}
