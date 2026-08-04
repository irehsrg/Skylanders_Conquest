// Skylanders Conquest - In-match pause menu implementation

#include "SkylandersPauseMenuWidget.h"
#include "SkylandersSettingsWidget.h"
#include "SkylandersUIStyle.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Blueprint/WidgetTree.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "InputCoreTypes.h"

void USkylandersPauseMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (!WidgetTree) return;

	SetIsFocusable(true);

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("PauseRoot"));
	WidgetTree->RootWidget = Root;

	UBorder* BG = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PauseBG"));
	BG->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.7f));
	UCanvasPanelSlot* BGSlot = Root->AddChildToCanvas(BG);
	BGSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	BGSlot->SetOffsets(FMargin(0));

	UTextBlock* Title = SkylandersUI::MakeText(WidgetTree, TEXT("PauseTitle"), TEXT("PAUSED"), 44, SkylandersUI::Gold);
	UCanvasPanelSlot* TitleSlot = Root->AddChildToCanvas(Title);
	TitleSlot->SetAnchors(FAnchors(0.5f, 0.25f, 0.5f, 0.25f));
	TitleSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	TitleSlot->SetAutoSize(true);

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PausePanel"));
	Panel->SetBrushColor(SkylandersUI::PanelBG);
	Panel->SetPadding(FMargin(24.f, 20.f));
	UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel);
	PanelSlot->SetAnchors(FAnchors(0.5f, 0.55f, 0.5f, 0.55f));
	PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	PanelSlot->SetSize(FVector2D(420.f, 330.f));
	PanelSlot->SetAutoSize(false);
	MenuPanel = Panel;

	UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PauseRows"));
	Panel->AddChild(Box);

	auto AddBtn = [&](UButton* Btn)
	{
		UVerticalBoxSlot* S = Box->AddChildToVerticalBox(Btn);
		S->SetPadding(FMargin(0.f, 7.f));
		S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		S->SetHorizontalAlignment(HAlign_Fill);
	};

	UButton* ResumeBtn = SkylandersUI::MakeButton(WidgetTree, TEXT("PauseResume"), TEXT("RESUME"));
	ResumeBtn->OnClicked.AddDynamic(this, &USkylandersPauseMenuWidget::OnResumeClicked);
	AddBtn(ResumeBtn);

	UButton* SettingsBtn = SkylandersUI::MakeButton(WidgetTree, TEXT("PauseSettings"), TEXT("SETTINGS"));
	SettingsBtn->OnClicked.AddDynamic(this, &USkylandersPauseMenuWidget::OnSettingsClicked);
	AddBtn(SettingsBtn);

	UButton* MenuBtn = SkylandersUI::MakeButton(WidgetTree, TEXT("PauseMainMenu"), TEXT("QUIT TO MAIN MENU"), 18);
	MenuBtn->OnClicked.AddDynamic(this, &USkylandersPauseMenuWidget::OnMainMenuClicked);
	AddBtn(MenuBtn);

	UButton* QuitBtn = SkylandersUI::MakeButton(WidgetTree, TEXT("PauseQuit"), TEXT("QUIT GAME"));
	QuitBtn->OnClicked.AddDynamic(this, &USkylandersPauseMenuWidget::OnQuitClicked);
	AddBtn(QuitBtn);
}

void USkylandersPauseMenuWidget::FocusWidget(UUserWidget* Widget)
{
	APlayerController* PC = GetOwningPlayer();
	if (!PC || !Widget) return;

	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(Widget->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PC->SetInputMode(InputMode);
	PC->bShowMouseCursor = true;
}

void USkylandersPauseMenuWidget::OnResumeClicked()
{
	Resume();
}

void USkylandersPauseMenuWidget::Resume()
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->SetPause(false);
	}

	RemoveFromParent();
	OnClosed.ExecuteIfBound();
}

void USkylandersPauseMenuWidget::OnSettingsClicked()
{
	SettingsWidget = CreateWidget<USkylandersSettingsWidget>(GetOwningPlayer(), USkylandersSettingsWidget::StaticClass());
	if (!SettingsWidget) return;

	SettingsWidget->OnClosed.BindUObject(this, &USkylandersPauseMenuWidget::HandleSettingsClosed);
	SettingsWidget->AddToViewport(300); // above the pause menu

	// Hide rather than remove, so returning does not rebuild the pause menu.
	SetVisibility(ESlateVisibility::Collapsed);
	FocusWidget(SettingsWidget);
}

void USkylandersPauseMenuWidget::HandleSettingsClosed()
{
	SettingsWidget = nullptr;
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	FocusWidget(this);
}

void USkylandersPauseMenuWidget::OnMainMenuClicked()
{
	UWorld* World = GetWorld();

	// Unpause before travelling, or the front-end loads into a paused world.
	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->SetPause(false);
	}

	RemoveFromParent();

	if (World)
	{
		UGameplayStatics::OpenLevel(World, MainMenuLevelName);
	}
}

void USkylandersPauseMenuWidget::OnQuitClicked()
{
	APlayerController* PC = GetOwningPlayer();
	if (PC)
	{
		PC->SetPause(false);
	}
	UKismetSystemLibrary::QuitGame(this, PC, EQuitPreference::Quit, false);
}

FReply USkylandersPauseMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// The pawn's input component is asleep while paused, so Escape has to be
	// handled here to close the menu.
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		Resume();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}
