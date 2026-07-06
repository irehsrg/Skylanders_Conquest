// Skylanders Conquest - Projectile Implementation

#include "SkylandersProjectile.h"
#include "SkylandersCharacter.h"
#include "SkylandersEnemy.h"
#include "SkylandersTower.h"
#include "SkylandersTitan.h"
#include "SkylandersMinion.h"
#include "SkylandersBuffCamp.h"
#include "SkylandersEnemyGod.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

ASkylandersProjectile::ASkylandersProjectile()
{
	PrimaryActorTick.bCanEverTick = false;

	// Create collision component
	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComponent"));
	CollisionComponent->InitSphereRadius(12.0f);
	CollisionComponent->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	CollisionComponent->SetNotifyRigidBodyCollision(true);

	// Overlap pawns instead of blocking — projectiles fly through characters
	// OnHit still fires for WorldStatic (walls) and WorldDynamic (towers/titans)
	// OnOverlap fires for Pawn objects (minions, enemies, enemy god)
	CollisionComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	CollisionComponent->SetGenerateOverlapEvents(true);

	RootComponent = CollisionComponent;

	// Create mesh component
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(CollisionComponent);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Create projectile movement component
	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->UpdatedComponent = CollisionComponent;
	ProjectileMovement->InitialSpeed = 3000.0f;
	ProjectileMovement->MaxSpeed = 3000.0f;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bShouldBounce = false;
	ProjectileMovement->ProjectileGravityScale = 0.0f;

	// Initialize settings
	InitialSpeed = 3000.0f;
	Lifetime = 0.35f;
	Damage = 20.0f;
	Lifesteal = 0.0f;
	bIsCrit = false;
	bDamagesStructures = true;
	ProjectileColor = FLinearColor(1.0f, 0.85f, 0.1f, 1.0f); // Gold
	VisualScale = 0.4f;      // Sphere default radius 50 -> 20
	CleaveRadius = 0.0f;     // No cleave by default
	CleaveDamageFraction = 0.5f;

	InitialLifeSpan = Lifetime;
}

void ASkylandersProjectile::BeginPlay()
{
	Super::BeginPlay();

	if (CollisionComponent)
	{
		CollisionComponent->OnComponentHit.AddDynamic(this, &ASkylandersProjectile::OnHit);
		CollisionComponent->OnComponentBeginOverlap.AddDynamic(this, &ASkylandersProjectile::OnOverlap);
	}

	// Fallback visual: characters without a projectile Blueprint (Hex, Tree Rex)
	// spawn this class directly, which has no mesh assigned — give it a small
	// colored sphere so shots are visible
	if (MeshComponent && !MeshComponent->GetStaticMesh())
	{
		if (UStaticMesh* SphereMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere")))
		{
			MeshComponent->SetStaticMesh(SphereMesh);
			MeshComponent->SetWorldScale3D(FVector(VisualScale));
			MeshComponent->SetCastShadow(false);
			if (UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial")))
			{
				UMaterialInstanceDynamic* DynMat = UMaterialInstanceDynamic::Create(BaseMat, this);
				DynMat->SetVectorParameterValue(FName("Color"), ProjectileColor);
				MeshComponent->SetMaterial(0, DynMat);
			}
		}
	}

	// Apply tuning knobs here — Blueprint Class Defaults are applied after the
	// constructor, so constructor-time InitialLifeSpan/InitialSpeed would ignore them
	SetLifeSpan(Lifetime);
	if (ProjectileMovement && InitialSpeed > 0.0f)
	{
		ProjectileMovement->MaxSpeed = InitialSpeed;
		ProjectileMovement->Velocity = GetActorForwardVector() * InitialSpeed;
	}

	UE_LOG(LogTemp, Log, TEXT("Projectile spawned! Instigator: %s"), GetInstigator() ? *GetInstigator()->GetName() : TEXT("NONE"));
}

// OnHit: fires for blocking collisions (walls, ground, buff camps)
void ASkylandersProjectile::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	// Buff camp (uses blocking collision)
	ASkylandersBuffCamp* BuffCamp = Cast<ASkylandersBuffCamp>(OtherActor);
	if (BuffCamp && !BuffCamp->bDead)
	{
		BuffCamp->TakeDamage_Custom(Damage, this);
		ApplyLifesteal();
	}

	// Hit a wall/ground/other blocking object — destroy
	Destroy();
}

// OnOverlap: fires for Pawn objects (minions, enemies, enemy god)
// Friendly minions are skipped — projectile keeps flying through them
void ASkylandersProjectile::OnOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor || OtherActor == this) return;

	// Skip the player who fired
	if (OtherActor == GetInstigator()) return;

	// Enemy minion — damage and destroy
	ASkylandersMinion* Minion = Cast<ASkylandersMinion>(OtherActor);
	if (Minion)
	{
		if (Minion->Team == ETowerTeam::Friendly || Minion->bDead) return; // Fly through friendly minions
		Minion->TakeDamage_Custom(Damage, this);
		ApplyLifesteal();
		CleaveNearby(OtherActor->GetActorLocation(), OtherActor);
		Destroy();
		return;
	}

	// Jungle enemy
	ASkylandersEnemy* Enemy = Cast<ASkylandersEnemy>(OtherActor);
	if (Enemy)
	{
		Enemy->TakeDamage_Custom(Damage, this);
		ApplyLifesteal();
		CleaveNearby(OtherActor->GetActorLocation(), OtherActor);
		Destroy();
		return;
	}

	// Enemy god
	ASkylandersEnemyGod* EnemyGod = Cast<ASkylandersEnemyGod>(OtherActor);
	if (EnemyGod && EnemyGod->CurrentState != EGodAIState::Dead)
	{
		EnemyGod->TakeDamage_Custom(Damage, this);
		ApplyLifesteal();
		CleaveNearby(OtherActor->GetActorLocation(), OtherActor);
		Destroy();
		return;
	}

	// Enemy tower — must be inside tower range to damage it
	ASkylandersTower* Tower = Cast<ASkylandersTower>(OtherActor);
	if (Tower)
	{
		if (Tower->Team == ETowerTeam::Friendly) return; // Fly through friendly towers
		if (!Tower->bDestroyed && bDamagesStructures)
		{
			APawn* Shooter = GetInstigator();
			if (Shooter)
			{
				float DistToTower = FVector::Dist(Shooter->GetActorLocation(), Tower->GetActorLocation());
				if (DistToTower <= Tower->AttackRange)
				{
					Tower->TakeDamage_Custom(Damage, this);
					ApplyLifesteal();
				}
			}
		}
		Destroy();
		return;
	}

	// Enemy titan — must be inside titan range to damage it
	ASkylandersTitan* Titan = Cast<ASkylandersTitan>(OtherActor);
	if (Titan)
	{
		if (Titan->Team == ETowerTeam::Friendly) return; // Fly through friendly titan
		if (!Titan->bDead && bDamagesStructures)
		{
			APawn* Shooter = GetInstigator();
			if (Shooter)
			{
				float DistToTitan = FVector::Dist(Shooter->GetActorLocation(), Titan->GetActorLocation());
				if (DistToTitan <= Titan->AttackRange)
				{
					Titan->TakeDamage_Custom(Damage, this);
					ApplyLifesteal();
				}
			}
		}
		Destroy();
		return;
	}
}

void ASkylandersProjectile::ApplyLifesteal()
{
	if (Lifesteal > 0.0f)
	{
		ASkylandersCharacter* Shooter = Cast<ASkylandersCharacter>(GetInstigator());
		if (Shooter && Shooter->CurrentHealth > 0.0f)
		{
			Shooter->Heal(Damage * Lifesteal);
		}
	}
}

void ASkylandersProjectile::CleaveNearby(const FVector& ImpactPoint, AActor* PrimaryTarget)
{
	if (CleaveRadius <= 0.0f) return;

	UWorld* World = GetWorld();
	if (!World) return;

	APawn* Shooter = GetInstigator();
	const float SplashDamage = Damage * CleaveDamageFraction;

	// Gather every enemy-side pawn type and splash the ones inside the radius
	auto InRange = [&](AActor* A) -> bool
	{
		return A && A != PrimaryTarget && A != Shooter
			&& FVector::Dist(A->GetActorLocation(), ImpactPoint) <= CleaveRadius;
	};

	TArray<AActor*> Found;

	UGameplayStatics::GetAllActorsOfClass(World, ASkylandersMinion::StaticClass(), Found);
	for (AActor* A : Found)
	{
		ASkylandersMinion* M = Cast<ASkylandersMinion>(A);
		if (M && M->Team == ETowerTeam::Enemy && !M->bDead && InRange(A))
			M->TakeDamage_Custom(SplashDamage, this);
	}

	Found.Reset();
	UGameplayStatics::GetAllActorsOfClass(World, ASkylandersEnemy::StaticClass(), Found);
	for (AActor* A : Found)
	{
		ASkylandersEnemy* E = Cast<ASkylandersEnemy>(A);
		if (E && E->CurrentHealth > 0.0f && InRange(A))
			E->TakeDamage_Custom(SplashDamage, this);
	}

	Found.Reset();
	UGameplayStatics::GetAllActorsOfClass(World, ASkylandersEnemyGod::StaticClass(), Found);
	for (AActor* A : Found)
	{
		ASkylandersEnemyGod* G = Cast<ASkylandersEnemyGod>(A);
		if (G && G->CurrentState != EGodAIState::Dead && InRange(A))
			G->TakeDamage_Custom(SplashDamage, this);
	}
}
