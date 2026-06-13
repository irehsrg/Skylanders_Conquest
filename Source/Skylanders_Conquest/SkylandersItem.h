// Skylanders Conquest - Item Data Structures

#pragma once

#include "CoreMinimal.h"
#include "SkylandersItem.generated.h"

// Stat bonuses an item can provide
USTRUCT(BlueprintType)
struct FSkylandersItemStats
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float Power = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float AttackSpeed = 0.0f; // Bonus shots per second

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float CritChance = 0.0f; // 0.0 - 1.0

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float MaxHealth = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float MaxMana = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float HealthRegen = 0.0f; // Per second

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float ManaRegen = 0.0f; // Per second

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float Protections = 0.0f; // Damage reduction (flat, applied via formula)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float Lifesteal = 0.0f; // 0.0 - 1.0

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float MovementSpeed = 0.0f; // Bonus units/sec

	// Add two stats together (for combining item bonuses)
	FSkylandersItemStats operator+(const FSkylandersItemStats& Other) const
	{
		FSkylandersItemStats Result;
		Result.Power = Power + Other.Power;
		Result.AttackSpeed = AttackSpeed + Other.AttackSpeed;
		Result.CritChance = CritChance + Other.CritChance;
		Result.MaxHealth = MaxHealth + Other.MaxHealth;
		Result.MaxMana = MaxMana + Other.MaxMana;
		Result.HealthRegen = HealthRegen + Other.HealthRegen;
		Result.ManaRegen = ManaRegen + Other.ManaRegen;
		Result.Protections = Protections + Other.Protections;
		Result.Lifesteal = Lifesteal + Other.Lifesteal;
		Result.MovementSpeed = MovementSpeed + Other.MovementSpeed;
		return Result;
	}
};

// Item category for shop filtering
UENUM(BlueprintType)
enum class ESkylandersItemCategory : uint8
{
	Offense		UMETA(DisplayName = "Offense"),
	Defense		UMETA(DisplayName = "Defense"),
	Utility		UMETA(DisplayName = "Utility")
};

// A single item definition
USTRUCT(BlueprintType)
struct FSkylandersItemData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	int32 ItemID = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	FString ItemName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	FString Description;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	int32 Cost = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	ESkylandersItemCategory Category = ESkylandersItemCategory::Offense;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	FSkylandersItemStats Stats;

	// Returns the sell price (67% of cost)
	int32 GetSellPrice() const { return FMath::FloorToInt(Cost * 0.67f); }

	bool IsValid() const { return ItemID > 0; }
};
