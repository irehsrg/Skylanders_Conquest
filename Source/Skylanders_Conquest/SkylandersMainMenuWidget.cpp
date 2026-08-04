// Skylanders Conquest - Front-End Main Menu Widget Implementation

#include "SkylandersMainMenuWidget.h"
#include "SkylandersCharacterSelectWidget.h"
#include "SkylandersSettingsWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Blueprint/WidgetTree.h"
#include "Fonts/SlateFontInfo.h"
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

	// Button column (centered). Character select and settings are both
	// full-screen overlays now, so no switcher is needed.
	UCanvasPanelSlot* MenuSlot = Root->AddChildToCanvas(BuildMainScreen());
	MenuSlot->SetAnchors(FAnchors(0.5f, 0.55f, 0.5f, 0.55f));
	MenuSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	MenuSlot->SetSize(FVector2D(420.f, 360.f));
	MenuSlot->SetAutoSize(false);
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

void USkylandersMainMenuWidget::OpenCharacterSelect()
{
	if (CharacterSelect && CharacterSelect->IsInViewport()) return;

	CharacterSelect = CreateWidget<USkylandersCharacterSelectWidget>(GetOwningPlayer(), USkylandersCharacterSelectWidget::StaticClass());
	if (CharacterSelect)
	{
		CharacterSelect->GameLevelName = GameLevelName;
		CharacterSelect->AddToViewport(10); // Above the main menu
	}
}

// ---- Handlers ----
void USkylandersMainMenuWidget::OnPlayClicked()
{
	OpenCharacterSelect();
}

void USkylandersMainMenuWidget::OnCharactersClicked()
{
	OpenCharacterSelect();
}

void USkylandersMainMenuWidget::OnSettingsClicked()
{
	SettingsWidget = CreateWidget<USkylandersSettingsWidget>(GetOwningPlayer(), USkylandersSettingsWidget::StaticClass());
	if (!SettingsWidget) return;

	SettingsWidget->OnClosed.BindUObject(this, &USkylandersMainMenuWidget::HandleSettingsClosed);
	SettingsWidget->AddToViewport(50); // above the front-end

	// Hide rather than remove, so returning does not rebuild the front-end.
	SetVisibility(ESlateVisibility::Collapsed);
	FocusWidget(SettingsWidget);
}

void USkylandersMainMenuWidget::HandleSettingsClosed()
{
	SettingsWidget = nullptr;
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	FocusWidget(this);
}

void USkylandersMainMenuWidget::FocusWidget(UUserWidget* Widget)
{
	APlayerController* PC = GetOwningPlayer();
	if (!PC || !Widget) return;

	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(Widget->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PC->SetInputMode(InputMode);
	PC->bShowMouseCursor = true;
}

void USkylandersMainMenuWidget::OnQuitClicked()
{
	APlayerController* PC = GetOwningPlayer();
	UKismetSystemLibrary::QuitGame(this, PC, EQuitPreference::Quit, false);
}
