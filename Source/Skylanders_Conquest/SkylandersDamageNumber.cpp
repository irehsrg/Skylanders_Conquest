// Skylanders Conquest - Floating Damage Number Implementation

#include "SkylandersDamageNumber.h"
#include "Components/TextRenderComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/CameraComponent.h"

ASkylandersDamageNumber::ASkylandersDamageNumber()
{
	PrimaryActorTick.bCanEverTick = true;

	TextComponent = CreateDefaultSubobject<UTextRenderComponent>(TEXT("TextComponent"));
	RootComponent = TextComponent;

	// Always face camera
	TextComponent->SetHorizontalAlignment(EHTA_Center);
	TextComponent->SetVerticalAlignment(EVRTA_TextCenter);
	TextComponent->SetWorldSize(30.0f);
	TextComponent->SetTextRenderColor(FColor::Yellow);

	// No collision
	TextComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Defaults
	FloatSpeed = 80.0f;
	Lifetime = 1.0f;
	ElapsedTime = 0.0f;
	BaseWorldSize = 30.0f;
	RandomOffset = FVector::ZeroVector;

	InitialLifeSpan = 2.0f; // Safety cleanup
}

void ASkylandersDamageNumber::SetDamageNumber(float Damage, FColor Color, bool bLargeText)
{
	if (!TextComponent) return;

	// Set the damage text
	int32 DamageInt = FMath::RoundToInt(Damage);
	TextComponent->SetText(FText::FromString(FString::Printf(TEXT("%d"), DamageInt)));
	TextComponent->SetTextRenderColor(Color);

	if (bLargeText)
	{
		BaseWorldSize = 50.0f; // Bigger for abilities/crits
		Lifetime = 1.4f;
	}
	else
	{
		BaseWorldSize = 30.0f;
		Lifetime = 1.0f;
	}
	TextComponent->SetWorldSize(BaseWorldSize);

	// Random horizontal offset so numbers don't stack
	RandomOffset = FVector(
		FMath::RandRange(-30.0f, 30.0f),
		FMath::RandRange(-30.0f, 30.0f),
		0.0f
	);

	SetActorLocation(GetActorLocation() + RandomOffset);
}

void ASkylandersDamageNumber::SetTextLabel(const FString& Label, FColor Color)
{
	if (!TextComponent) return;

	TextComponent->SetText(FText::FromString(Label));
	TextComponent->SetTextRenderColor(Color);
	BaseWorldSize = 30.0f;
	TextComponent->SetWorldSize(BaseWorldSize);
	Lifetime = 1.0f;

	RandomOffset = FVector(
		FMath::RandRange(-30.0f, 30.0f),
		FMath::RandRange(-30.0f, 30.0f),
		0.0f
	);

	SetActorLocation(GetActorLocation() + RandomOffset);
}

void ASkylandersDamageNumber::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	ElapsedTime += DeltaTime;

	// Face camera
	APlayerCameraManager* CamMgr = UGameplayStatics::GetPlayerCameraManager(GetWorld(), 0);
	if (CamMgr)
	{
		FVector CamLoc = CamMgr->GetCameraLocation();
		FVector ToCamera = CamLoc - GetActorLocation();
		FRotator FaceRot = ToCamera.Rotation();
		SetActorRotation(FaceRot);
	}

	// Float upward
	FVector NewLocation = GetActorLocation();
	NewLocation.Z += FloatSpeed * DeltaTime;
	SetActorLocation(NewLocation);

	// Slow down over time
	FloatSpeed = FMath::Max(FloatSpeed - 40.0f * DeltaTime, 20.0f);

	// Fade out in the second half of lifetime
	float Alpha = 1.0f;
	if (ElapsedTime > Lifetime * 0.5f)
	{
		Alpha = 1.0f - ((ElapsedTime - Lifetime * 0.5f) / (Lifetime * 0.5f));
		Alpha = FMath::Clamp(Alpha, 0.0f, 1.0f);
	}

	if (TextComponent)
	{
		// Scale down to simulate fade (TextRenderComponent doesn't support opacity)
		float Scale = FMath::Clamp(Alpha, 0.1f, 1.0f);
		TextComponent->SetWorldSize(BaseWorldSize * Scale);
		TextComponent->SetVisibility(Alpha > 0.05f);
	}

	// Destroy when done
	if (ElapsedTime >= Lifetime)
	{
		Destroy();
	}
}
