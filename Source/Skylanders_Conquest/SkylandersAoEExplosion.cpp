// Skylanders Conquest - AoE Explosion Implementation

#include "SkylandersAoEExplosion.h"
#include "SkylandersEnemy.h"
#include "SkylandersMinion.h"
#include "SkylandersBuffCamp.h"
#include "SkylandersEnemyGod.h"
#include "SkylandersTower.h"
#include "SkylandersTitan.h"
#include "SkylandersCharacter.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"

ASkylandersAoEExplosion::ASkylandersAoEExplosion()
{
	PrimaryActorTick.bCanEverTick = false;

	// Collision sphere (no physics, just visual reference)
	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
	CollisionSphere->InitSphereRadius(20.0f);
	CollisionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RootComponent = CollisionSphere;

	// Visual indicator - gold sphere
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(CollisionSphere);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComponent->SetRelativeScale3D(FVector(0.5f, 0.5f, 0.5f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere"));
	if (SphereMesh.Succeeded())
	{
		MeshComponent->SetStaticMesh(SphereMesh.Object);
	}

	// Defaults
	DamageAmount = 80.0f;
	ExplosionRadius = 300.0f;
	DetonationDelay = 0.5f;

	InitialLifeSpan = 3.0f; // Cleanup safety
}

void ASkylandersAoEExplosion::BeginPlay()
{
	Super::BeginPlay();

	// Warning circle on the ground - flat yellow cylinder
	{
		FActorSpawnParameters WarnParams;
		WarnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		FVector WarnLoc = GetActorLocation();
		WarnLoc.Z += 2.0f; // Slightly above ground
		AActor* WarnVFX = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), WarnLoc, FRotator::ZeroRotator, WarnParams);
		if (WarnVFX)
		{
			UStaticMeshComponent* WarnMesh = NewObject<UStaticMeshComponent>(WarnVFX);
			WarnVFX->SetRootComponent(WarnMesh);
			WarnMesh->RegisterComponent();
			UStaticMesh* CylMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder"));
			if (CylMesh) WarnMesh->SetStaticMesh(CylMesh);
			// Cylinder default radius = 50, so scale XY = ExplosionRadius / 50
			float CylScale = ExplosionRadius / 50.0f;
			WarnMesh->SetWorldScale3D(FVector(CylScale, CylScale, 0.02f));
			WarnMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			WarnMesh->SetCastShadow(false);
			UMaterialInterface* WarnBaseMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial"));
			if (WarnBaseMat)
			{
				UMaterialInstanceDynamic* WarnDynMat = UMaterialInstanceDynamic::Create(WarnBaseMat, WarnVFX);
				WarnDynMat->SetVectorParameterValue(FName("Color"), FLinearColor(1.0f, 0.85f, 0.0f)); // Gold warning
				WarnMesh->SetMaterial(0, WarnDynMat);
			}
			WarnVFX->SetLifeSpan(DetonationDelay + 0.1f); // Match detonation timing
		}
	}

	// Set the existing gold sphere mesh indicator to full radius for visibility
	if (MeshComponent)
	{
		float IndicatorScale = ExplosionRadius / 100.0f; // Sphere default radius 50, diameter 100
		MeshComponent->SetRelativeScale3D(FVector(IndicatorScale * 0.3f, IndicatorScale * 0.3f, 0.05f));
	}

	// Detonate after delay
	GetWorld()->GetTimerManager().SetTimer(DetonationTimer, this, &ASkylandersAoEExplosion::Detonate, DetonationDelay, false);
}

void ASkylandersAoEExplosion::Detonate()
{
	UE_LOG(LogTemp, Log, TEXT("AoE Explosion detonated at %s! Radius: %.0f, Damage: %.0f"),
		*GetActorLocation().ToString(), ExplosionRadius, DamageAmount);

	// Explosion visual - spawned orange sphere that auto-destroys
	{
		FActorSpawnParameters ExpVFXParams;
		ExpVFXParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AActor* ExpVFX = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), GetActorLocation(), FRotator::ZeroRotator, ExpVFXParams);
		if (ExpVFX)
		{
			UStaticMeshComponent* ExpMesh = NewObject<UStaticMeshComponent>(ExpVFX);
			ExpVFX->SetRootComponent(ExpMesh);
			ExpMesh->RegisterComponent();
			UStaticMesh* SphereMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere"));
			if (SphereMesh) ExpMesh->SetStaticMesh(SphereMesh);
			// Sphere default radius = 50, scale to ExplosionRadius
			float ExpScale = ExplosionRadius / 50.0f;
			ExpMesh->SetWorldScale3D(FVector(ExpScale));
			ExpMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			ExpMesh->SetCastShadow(false);
			UMaterialInterface* ExpBaseMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial"));
			if (ExpBaseMat)
			{
				UMaterialInstanceDynamic* ExpDynMat = UMaterialInstanceDynamic::Create(ExpBaseMat, ExpVFX);
				ExpDynMat->SetVectorParameterValue(FName("Color"), FLinearColor(1.0f, 0.6f, 0.0f)); // Orange explosion
				ExpMesh->SetMaterial(0, ExpDynMat);
			}
			ExpVFX->SetLifeSpan(0.5f); // Auto-destroy after 0.5s
		}
	}

	// Find all enemies in radius
	TArray<AActor*> AllEnemies;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASkylandersEnemy::StaticClass(), AllEnemies);

	for (AActor* Actor : AllEnemies)
	{
		float Distance = FVector::Dist(GetActorLocation(), Actor->GetActorLocation());
		if (Distance <= ExplosionRadius)
		{
			ASkylandersEnemy* Enemy = Cast<ASkylandersEnemy>(Actor);
			if (Enemy && Enemy->CurrentHealth > 0.0f)
			{
				// Damage falls off with distance (full at center, 50% at edge)
				float DamageFalloff = 1.0f - (Distance / ExplosionRadius) * 0.5f;
				float FinalDamage = DamageAmount * DamageFalloff;
				Enemy->TakeDamage_Custom(FinalDamage, GetInstigator());
				UE_LOG(LogTemp, Log, TEXT("AoE hit enemy '%s' for %.0f damage (%.0f%% falloff)"),
					*Enemy->EnemyName, FinalDamage, DamageFalloff * 100.0f);

				// Give coins to instigator
				ASkylandersCharacter* SkyChar = Cast<ASkylandersCharacter>(GetInstigator());
				if (SkyChar)
				{
					SkyChar->AddCoins(3);
				}
			}
		}
	}

	// Also damage enemy minions in radius
	TArray<AActor*> AllMinions;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASkylandersMinion::StaticClass(), AllMinions);

	for (AActor* Actor : AllMinions)
	{
		ASkylandersMinion* Minion = Cast<ASkylandersMinion>(Actor);
		if (Minion && Minion->Team == ETowerTeam::Enemy && !Minion->bDead)
		{
			float Distance = FVector::Dist(GetActorLocation(), Actor->GetActorLocation());
			if (Distance <= ExplosionRadius)
			{
				float DamageFalloff = 1.0f - (Distance / ExplosionRadius) * 0.5f;
				float FinalDamage = DamageAmount * DamageFalloff;
				Minion->TakeDamage_Custom(FinalDamage, GetInstigator());
				UE_LOG(LogTemp, Log, TEXT("AoE hit minion for %.0f damage (%.0f%% falloff)"),
					FinalDamage, DamageFalloff * 100.0f);
			}
		}
	}

	// Also damage buff camps in radius
	TArray<AActor*> AllBuffCamps;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASkylandersBuffCamp::StaticClass(), AllBuffCamps);

	for (AActor* Actor : AllBuffCamps)
	{
		ASkylandersBuffCamp* BuffCamp = Cast<ASkylandersBuffCamp>(Actor);
		if (BuffCamp && !BuffCamp->bDead)
		{
			float Distance = FVector::Dist(GetActorLocation(), Actor->GetActorLocation());
			if (Distance <= ExplosionRadius)
			{
				float DamageFalloff = 1.0f - (Distance / ExplosionRadius) * 0.5f;
				float FinalDamage = DamageAmount * DamageFalloff;
				BuffCamp->TakeDamage_Custom(FinalDamage, GetInstigator());
				UE_LOG(LogTemp, Log, TEXT("AoE hit buff camp '%s' for %.0f damage (%.0f%% falloff)"),
					*BuffCamp->CampName, FinalDamage, DamageFalloff * 100.0f);
			}
		}
	}

	// Also damage enemy gods in radius
	TArray<AActor*> AllEnemyGods;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASkylandersEnemyGod::StaticClass(), AllEnemyGods);

	for (AActor* Actor : AllEnemyGods)
	{
		ASkylandersEnemyGod* EnemyGod = Cast<ASkylandersEnemyGod>(Actor);
		if (EnemyGod && EnemyGod->CurrentState != EGodAIState::Dead)
		{
			float Distance = FVector::Dist(GetActorLocation(), Actor->GetActorLocation());
			if (Distance <= ExplosionRadius)
			{
				float DamageFalloff = 1.0f - (Distance / ExplosionRadius) * 0.5f;
				float FinalDamage = DamageAmount * DamageFalloff;
				EnemyGod->TakeDamage_Custom(FinalDamage, GetInstigator());
				UE_LOG(LogTemp, Log, TEXT("AoE hit enemy god '%s' for %.0f damage (%.0f%% falloff)"),
					*EnemyGod->GodName, FinalDamage, DamageFalloff * 100.0f);
			}
		}
	}

	Destroy();
}
