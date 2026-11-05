// Skylanders Conquest - Front-End Main Menu Widget Implementation

#include "SkylandersMainMenuWidget.h"
#include "SkylandersCharacterSelectWidget.h"
#include "SkylandersSettingsWidget.h"
#include "SkylandersUIStyle.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Blueprint/WidgetTree.h"
#include "Fonts/SlateFontInfo.h"
#include "Kismet/KismetSystemLibrary.h"
#include "GameFramework/PlayerController.h"

// Gold rim on the big CONQUEST word: a dark outline would vanish on ScreenBG
static const FLinearColor MenuTitleOutline(0.95f, 0.78f, 0.20f, 0.55f);

void USkylandersMainMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (!WidgetTree) return;

	// Root canvas
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("MenuRoot"));
	WidgetTree->RootWidget = Root;

	// Opaque full-screen backdrop
	UBorder* BG = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MenuBG"));
	BG->SetBrushColor(SkylandersUI::ScreenBG);
	UCanvasPanelSlot* BGSlot = Root->AddChildToCanvas(BG);
	BGSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	BGSlot->SetOffsets(FMargin(0));

	// ---- Title block, left-aligned with the button column ----
	UVerticalBox* TitleBlock = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MenuTitleBlock"));
	UCanvasPanelSlot* TitleSlot = Root->AddChildToCanvas(TitleBlock);
	TitleSlot->SetAnchors(FAnchors(0.08f, 0.14f, 0.08f, 0.14f));
	TitleSlot->SetAlignment(FVector2D(0.f, 0.f));
	TitleSlot->SetAutoSize(true);

	UTextBlock* Kicker = SkylandersUI::MakeText(WidgetTree, TEXT("MenuTitleKicker"), TEXT("SKYLANDERS"), 24,
		SkylandersUI::Gold, ETextJustify::Left);
	{
		// Wide tracking so the small word spans most of the big one
		SkylandersUI::SetFontStyle(Kicker, true, false);
		FSlateFontInfo Font = Kicker->GetFont();
		Font.LetterSpacing = 520;
		Kicker->SetFont(Font);
	}
	TitleBlock->AddChildToVerticalBox(Kicker)->SetPadding(FMargin(4.f, 0.f, 0.f, 0.f));

	UTextBlock* Title = SkylandersUI::MakeText(WidgetTree, TEXT("MenuTitle"), TEXT("CONQUEST"), 84,
		SkylandersUI::White, ETextJustify::Left);
	SkylandersUI::SetFontStyle(Title, true, true, MenuTitleOutline);
	TitleBlock->AddChildToVerticalBox(Title)->SetPadding(FMargin(0.f, -10.f, 0.f, 6.f));

	// Short gold rule under the title
	{
		UBorder* Rule = SkylandersUI::MakePanel(WidgetTree, TEXT("MenuTitleRule"), SkylandersUI::Gold, 1.f,
			FLinearColor::Transparent, 0.f, FMargin(0));
		USizeBox* RuleBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("MenuTitleRuleBox"));
		RuleBox->SetWidthOverride(140.f);
		RuleBox->SetHeightOverride(3.f);
		RuleBox->AddChild(Rule);
		UVerticalBoxSlot* RuleSlot = TitleBlock->AddChildToVerticalBox(RuleBox);
		RuleSlot->SetHorizontalAlignment(HAlign_Left);
		RuleSlot->SetPadding(FMargin(4.f, 0.f, 0.f, 10.f));
	}

	UTextBlock* Subtitle = SkylandersUI::MakeText(WidgetTree, TEXT("MenuSubtitle"), TEXT("A Skylanders MOBA"), 18,
		SkylandersUI::Dim, ETextJustify::Left);
	TitleBlock->AddChildToVerticalBox(Subtitle)->SetPadding(FMargin(4.f, 0.f, 0.f, 0.f));

	// ---- Button column in the left third. Character select and settings are
	// both full-screen overlays, so no switcher is needed. ----
	UCanvasPanelSlot* MenuSlot = Root->AddChildToCanvas(BuildMainScreen());
	MenuSlot->SetAnchors(FAnchors(0.08f, 0.46f, 0.08f, 0.46f));
	MenuSlot->SetAlignment(FVector2D(0.f, 0.f));
	MenuSlot->SetSize(FVector2D(380.f, 300.f));
	MenuSlot->SetAutoSize(false);

	// ---- Footer ----
	UTextBlock* Footer = SkylandersUI::MakeText(WidgetTree, TEXT("MenuFooter"), TEXT("Prototype · UE 5.8"), 12,
		SkylandersUI::Dim, ETextJustify::Left);
	UCanvasPanelSlot* FooterSlot = Root->AddChildToCanvas(Footer);
	FooterSlot->SetAnchors(FAnchors(0.08f, 0.96f, 0.08f, 0.96f));
	FooterSlot->SetAlignment(FVector2D(0.f, 1.f));
	FooterSlot->SetAutoSize(true);
}

UVerticalBox* USkylandersMainMenuWidget::BuildMainScreen()
{
	UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MainScreen"));

	auto AddBtn = [&](UButton* Btn)
	{
		UVerticalBoxSlot* S = Box->AddChildToVerticalBox(Btn);
		S->SetPadding(FMargin(0.f, 7.f));
		S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		S->SetHorizontalAlignment(HAlign_Fill);
	};

	UButton* PlayBtn = SkylandersUI::MakeButton(WidgetTree, TEXT("PlayBtn"), TEXT("PLAY"), 22);
	PlayBtn->OnClicked.AddDynamic(this, &USkylandersMainMenuWidget::OnPlayClicked);
	// PLAY is the primary action: gold rim and gold label
	SkylandersUI::SetButtonHighlight(PlayBtn, true);
	if (UTextBlock* PlayLabel = SkylandersUI::GetButtonLabel(PlayBtn))
	{
		PlayLabel->SetColorAndOpacity(FSlateColor(SkylandersUI::Gold));
		SkylandersUI::SetFontStyle(PlayLabel, true, false);
	}
	AddBtn(PlayBtn);

	UButton* CharBtn = SkylandersUI::MakeButton(WidgetTree, TEXT("CharBtn"), TEXT("CHARACTERS"), 22);
	CharBtn->OnClicked.AddDynamic(this, &USkylandersMainMenuWidget::OnCharactersClicked);
	AddBtn(CharBtn);

	UButton* SetBtn = SkylandersUI::MakeButton(WidgetTree, TEXT("SetBtn"), TEXT("SETTINGS"), 22);
	SetBtn->OnClicked.AddDynamic(this, &USkylandersMainMenuWidget::OnSettingsClicked);
	AddBtn(SetBtn);

	UButton* QuitBtn = SkylandersUI::MakeButton(WidgetTree, TEXT("QuitBtn"), TEXT("QUIT"), 22);
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
