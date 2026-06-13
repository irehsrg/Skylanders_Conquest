// Simple enemy health bar widget

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "EnemyHealthBarWidget.generated.h"

UCLASS()
class SKYLANDERS_CONQUEST_API UEnemyHealthBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable)
	void SetHealth(float Current, float Max);

	UFUNCTION(BlueprintCallable)
	void SetEnemyName(const FString& Name);

	UPROPERTY(BlueprintReadOnly)
	float DisplayHealth;

	UPROPERTY(BlueprintReadOnly)
	float DisplayMaxHealth;

	UPROPERTY(BlueprintReadOnly)
	float DisplayPercent;

	UPROPERTY(BlueprintReadOnly)
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly)
	FString DisplayHealthText;
};
