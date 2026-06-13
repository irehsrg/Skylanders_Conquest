// Skylanders Conquest - Front-End Main Menu Widget Implementation

#include "SkylandersMainMenuWidget.h"
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

UVerticalBox* USkylandersMainMenuWidget::BuildCharacterScreen()
{
	UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("CharScreen"));

	UTextBlock* Header = MakeText(WidgetTree, TEXT("CharHeader"), TEXT("SELECT YOUR SKYLANDER"), 24, MenuGold);
	Box->AddChildToVerticalBox(Header)->SetPadding(FMargin(0.f, 0.f, 0.f, 16.f));

	// Trigger Happy (only unlocked character)
	UButton* TH = MakeMenuButton(TEXT("TRIGGER HAPPY"), TEXT("CharTH"));
	TH->OnClicked.AddDynamic(this, &USkylandersMainMenuWidget::OnSelectTriggerHappy);
	Box->AddChildToVerticalBox(TH)->SetPadding(FMargin(0.f, 6.f));

	// Locked placeholders for future roster
	UButton* Locked1 = MakeMenuButton(TEXT("??? (LOCKED)"), TEXT("CharLock1"));
	Locked1->SetIsEnabled(false);
	Box->AddChildToVerticalBox(Locked1)->SetPadding(FMargin(0.f, 6.f));

	UButton* Locked2 = MakeMenuButton(TEXT("??? (LOCKED)"), TEXT("CharLock2"));
	Locked2->SetIsEnabled(false);
	Box->AddChildToVerticalBox(Locked2)->SetPadding(FMargin(0.f, 6.f));

	UButton* Back = MakeMenuButton(TEXT("BACK"), TEXT("CharBack"));
	Back->OnClicked.AddDynamic(this, &USkylandersMainMenuWidget::OnBackClicked);
	Box->AddChildToVerticalBox(Back)->SetPadding(FMargin(0.f, 18.f, 0.f, 0.f));

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

void USkylandersMainMenuWidget::OnSelectTriggerHappy()
{
	// Only one character for now; selecting just starts the match.
	UGameplayStatics::OpenLevel(this, GameLevelName);
}

void USkylandersMainMenuWidget::OnQuitClicked()
{
	APlayerController* PC = GetOwningPlayer();
	UKismetSystemLibrary::QuitGame(this, PC, EQuitPreference::Quit, false);
}
