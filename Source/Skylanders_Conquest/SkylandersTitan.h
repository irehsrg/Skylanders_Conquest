// Skylanders Conquest - Titan (Base Boss / Win Condition)

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SkylandersTower.h"
#include "SkylandersTitan.generated.h"

class UStaticMeshComponent;
class UWidgetComponent;
class UProgressBar;
class UTextBlock;
class USkylandersVictoryWidget;
class USoundBase;
class USceneComponent;
class UMaterialInstanceDynamic;

UCLASS()
class SKYLANDERS_CONQUEST_API ASkylandersTitan : public AActor
{
	GENERATED_BODY()

public:
	ASkylandersTitan();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	virtual void Tick(float DeltaTime) override;

	// ========== COMPONENTS ==========

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* BodyMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* HeadMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI")
	UWidgetComponent* HealthBarComp;

	// ========== STATS ==========

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float MaxHealth;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float CurrentHealth;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	FString TitanName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float AttackDamage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float AttackRange;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float AttackCooldown;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	ETowerTeam Team;

	// The tower protecting this titan - must be destroyed before titan can be damaged
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	ASkylandersTower* ProtectingTower;

	float AttackTimer;
	AActor* CurrentTarget;
	bool bDead;

	// ========== AUDIO ==========

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	USoundBase* AttackSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	USoundBase* DestroySound;

	// ========== FUNCTIONS ==========

	UFUNCTION(BlueprintCallable, Category = "Stats")
	void TakeDamage_Custom(float DamageAmount, AActor* DamageCauser);

	bool IsVulnerable() const;
	void Die();
	void FindTarget();
	void AttackTarget();
	void UpdateHealthBar();
	void ShowEndScreen(bool bVictory);

	// End screen widget
	UPROPERTY()
	USkylandersVictoryWidget* VictoryWidget;

	// Health bar cache
	bool bHealthBarInitialized;
	UProgressBar* CachedHealthBar;
	UTextBlock* CachedNameText;
	UTextBlock* CachedHealthText;

	// ========== VISUALS (code-built, not exposed) ==========
	// BodyMesh is the thick blocking column, HeadMesh its capital. Around them: a
	// two-tier plinth, four pillars with glowing caps, a flared collar and a big
	// floating core crystal that pulses while the titan is vulnerable.

	// Cancels BodyMesh's scale so every part below is laid out in world units
	UPROPERTY() USceneComponent* VisualRoot;
	UPROPERTY() TArray<UStaticMeshComponent*> PlinthParts;
	UPROPERTY() TArray<UStaticMeshComponent*> PillarParts;
	UPROPERTY() TArray<UStaticMeshComponent*> PillarCaps;
	UPROPERTY() UStaticMeshComponent* CollarMesh;
	UPROPERTY() USceneComponent* CorePivot;
	UPROPERTY() TArray<UStaticMeshComponent*> CoreParts;
	UPROPERTY() UMaterialInstanceDynamic* CoreMID;

	float CoreTime;
	float CoreBaseZ;
	// -1 = not yet applied, else the last vulnerability the core was colored for
	int32 CoreShownVulnerable;

	void LayoutVisuals();
};
