// Simple enemy health bar widget

#include "EnemyHealthBarWidget.h"

void UEnemyHealthBarWidget::SetHealth(float Current, float Max)
{
	DisplayHealth = Current;
	DisplayMaxHealth = Max;
	DisplayPercent = (Max > 0.0f) ? (Current / Max) : 0.0f;
	DisplayHealthText = FString::Printf(TEXT("%.0f / %.0f"), Current, Max);
}

void UEnemyHealthBarWidget::SetEnemyName(const FString& Name)
{
	DisplayName = Name;
}
