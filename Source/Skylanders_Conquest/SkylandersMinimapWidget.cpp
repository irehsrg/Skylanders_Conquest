// Skylanders Conquest - Minimap Widget Implementation

#include "SkylandersMinimapWidget.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Kismet/GameplayStatics.h"
#include "SkylandersCharacter.h"
#include "SkylandersMinion.h"
#include "SkylandersEnemy.h"
#include "SkylandersEnemyGod.h"
#include "SkylandersTower.h"
#include "SkylandersTitan.h"
#include "SkylandersBuffCamp.h"

void USkylandersMinimapWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree) return;

	// Root canvas
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("MMRoot"));
	WidgetTree->RootWidget = Root;

	// Background border anchored top-right
	UBorder* BG = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MMBG"));
	BG->SetBrushColor(FLinearColor(0.01f, 0.01f, 0.03f, 0.85f));
	BG->SetPadding(FMargin(2.0f));

	UCanvasPanelSlot* BGSlot = Root->AddChildToCanvas(BG);
	BGSlot->SetAnchors(FAnchors(1.0f, 0.0f, 1.0f, 0.0f));
	BGSlot->SetAlignment(FVector2D(1.0f, 0.0f));
	BGSlot->SetPosition(FVector2D(-10.0f, 60.0f));
	BGSlot->SetSize(FVector2D(MinimapSize + 4.0f, MinimapSize + 4.0f));

	// Inner canvas for dots
	DotCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("MMDotCanvas"));
	BG->AddChild(DotCanvas);

	// Pre-allocate dot widgets
	Dots.SetNum(MaxDots);
	DotSlots.SetNum(MaxDots);

	for (int32 i = 0; i < MaxDots; i++)
	{
		UBorder* Dot = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),
			*FString::Printf(TEXT("MMDot_%d"), i));
		Dot->SetBrushColor(FLinearColor::White);
		Dot->SetVisibility(ESlateVisibility::Collapsed);
		Dot->SetPadding(FMargin(0));

		UCanvasPanelSlot* DSlot = DotCanvas->AddChildToCanvas(Dot);
		DSlot->SetSize(FVector2D(4.0f, 4.0f));
		DSlot->SetAutoSize(false);

		Dots[i] = Dot;
		DotSlots[i] = DSlot;
	}

	ActiveDotCount = 0;
}

void USkylandersMinimapWidget::PlaceDot(int32& Index, FVector WorldPos, FLinearColor Color, FVector2D Size)
{
	if (Index >= MaxDots) return;

	// Map world coords to minimap coords
	// World X → minimap Y (inverted, up=positive X), World Y → minimap X
	float MX = ((WorldPos.Y - MapWorldCenter.Y) / MapHalfExtent) * (MinimapSize * 0.5f) + (MinimapSize * 0.5f);
	float MY = MinimapSize - (((WorldPos.X - MapWorldCenter.X) / MapHalfExtent) * (MinimapSize * 0.5f) + (MinimapSize * 0.5f));

	// Skip if outside minimap bounds
	if (MX < -Size.X || MX > MinimapSize + Size.X || MY < -Size.Y || MY > MinimapSize + Size.Y)
	{
		return; // Don't consume a dot for off-map entities
	}

	MX = FMath::Clamp(MX, 0.0f, MinimapSize);
	MY = FMath::Clamp(MY, 0.0f, MinimapSize);

	Dots[Index]->SetBrushColor(Color);
	Dots[Index]->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	DotSlots[Index]->SetPosition(FVector2D(MX - Size.X * 0.5f, MY - Size.Y * 0.5f));
	DotSlots[Index]->SetSize(Size);
	Index++;
}

void USkylandersMinimapWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	UWorld* World = GetWorld();
	if (!World) return;

	int32 Idx = 0;

	// Player (green, large)
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(World, 0);
	ASkylandersCharacter* Player = Cast<ASkylandersCharacter>(PlayerPawn);
	if (Player && Player->CurrentHealth > 0.0f)
	{
		PlaceDot(Idx, Player->GetActorLocation(), FLinearColor(0.0f, 1.0f, 0.3f), FVector2D(8, 8));
	}

	// Enemy God (red, large)
	TArray<AActor*> AllGods;
	UGameplayStatics::GetAllActorsOfClass(World, ASkylandersEnemyGod::StaticClass(), AllGods);
	for (AActor* A : AllGods)
	{
		ASkylandersEnemyGod* God = Cast<ASkylandersEnemyGod>(A);
		if (God && God->CurrentState != EGodAIState::Dead)
		{
			PlaceDot(Idx, A->GetActorLocation(), FLinearColor(1.0f, 0.1f, 0.1f), FVector2D(8, 8));
		}
	}

	// Towers (medium squares)
	TArray<AActor*> AllTowers;
	UGameplayStatics::GetAllActorsOfClass(World, ASkylandersTower::StaticClass(), AllTowers);
	for (AActor* A : AllTowers)
	{
		ASkylandersTower* T = Cast<ASkylandersTower>(A);
		if (T && !T->bDestroyed)
		{
			FLinearColor C = (T->Team == ETowerTeam::Friendly)
				? FLinearColor(0.2f, 0.4f, 1.0f) : FLinearColor(1.0f, 0.3f, 0.2f);
			PlaceDot(Idx, A->GetActorLocation(), C, FVector2D(10, 10));
		}
	}

	// Titans (large squares)
	TArray<AActor*> AllTitans;
	UGameplayStatics::GetAllActorsOfClass(World, ASkylandersTitan::StaticClass(), AllTitans);
	for (AActor* A : AllTitans)
	{
		ASkylandersTitan* T = Cast<ASkylandersTitan>(A);
		if (T && !T->bDead)
		{
			FLinearColor C = (T->Team == ETowerTeam::Friendly)
				? FLinearColor(0.3f, 0.3f, 1.0f) : FLinearColor(1.0f, 0.2f, 0.6f);
			PlaceDot(Idx, A->GetActorLocation(), C, FVector2D(12, 12));
		}
	}

	// Minions (small dots)
	TArray<AActor*> AllMinions;
	UGameplayStatics::GetAllActorsOfClass(World, ASkylandersMinion::StaticClass(), AllMinions);
	for (AActor* A : AllMinions)
	{
		ASkylandersMinion* M = Cast<ASkylandersMinion>(A);
		if (M && !M->bDead)
		{
			FLinearColor C = (M->Team == ETowerTeam::Friendly)
				? FLinearColor(0.3f, 0.5f, 1.0f) : FLinearColor(1.0f, 0.4f, 0.3f);
			PlaceDot(Idx, A->GetActorLocation(), C, FVector2D(3, 3));
		}
	}

	// Buff camps (yellow, medium)
	TArray<AActor*> AllBuffs;
	UGameplayStatics::GetAllActorsOfClass(World, ASkylandersBuffCamp::StaticClass(), AllBuffs);
	for (AActor* A : AllBuffs)
	{
		ASkylandersBuffCamp* B = Cast<ASkylandersBuffCamp>(A);
		if (B && !B->bDead)
		{
			PlaceDot(Idx, A->GetActorLocation(), FLinearColor(1.0f, 0.9f, 0.2f), FVector2D(6, 6));
		}
	}

	// Hide unused dots
	for (int32 i = Idx; i < MaxDots; i++)
	{
		Dots[i]->SetVisibility(ESlateVisibility::Collapsed);
	}
}
