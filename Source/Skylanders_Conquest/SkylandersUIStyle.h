// Skylanders Conquest - Shared menu styling helpers

#pragma once

#include "CoreMinimal.h"
#include "Types/SlateEnums.h"

class UButton;
class UTextBlock;
class UWidgetTree;

/**
 * Small widget factories so the front-end, pause menu, settings screen and end
 * screen share one look instead of each re-declaring the same brushes and fonts.
 */
namespace SkylandersUI
{
	extern const FLinearColor Gold;
	extern const FLinearColor White;
	extern const FLinearColor Dim;

	/** Opaque background for a full-screen menu (front-end). */
	extern const FLinearColor ScreenBG;

	/** Darkened background for menus layered over gameplay (pause / settings in match). */
	extern const FLinearColor OverlayBG;

	/** Fill for the panel that holds a screen's rows. */
	extern const FLinearColor PanelBG;

	UTextBlock* MakeText(UWidgetTree* Tree, FName Name, const FString& Str, int32 FontSize,
		FLinearColor Color, ETextJustify::Type Justify = ETextJustify::Center);

	/** Menu button using the shared normal/hover/pressed brushes, with a centered label. */
	UButton* MakeButton(UWidgetTree* Tree, FName Name, const FString& Label, int32 FontSize = 22);

	/** The label inside a button built by MakeButton; use it to retitle a button at runtime. */
	UTextBlock* GetButtonLabel(UButton* Btn);
}
