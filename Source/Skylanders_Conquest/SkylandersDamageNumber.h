// Skylanders Conquest - Floating Damage Number

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SkylandersDamageNumber.generated.h"

class UTextRenderComponent;

UCLASS()
class SKYLANDERS_CONQUEST_API ASkylandersDamageNumber : public AActor
{
	GENERATED_BODY()

public:
	ASkylandersDamageNumber();

	virtual void Tick(float DeltaTime) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	UTextRenderComponent* TextComponent;

	// Settings
	float FloatSpeed;
	float Lifetime;
	float ElapsedTime;
	float BaseWorldSize;
	FVector RandomOffset;

	// Initialize with damage value and color
	void SetDamageNumber(float Damage, FColor Color, bool bLargeText = false);

	// Initialize with arbitrary text (e.g. "IMMUNE")
	void SetTextLabel(const FString& Label, FColor Color);
};
