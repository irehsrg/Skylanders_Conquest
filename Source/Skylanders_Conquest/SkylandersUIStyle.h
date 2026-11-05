// Skylanders Conquest - Shared menu styling helpers

#pragma once

#include "CoreMinimal.h"
#include "Types/SlateEnums.h"
#include "Styling/SlateBrush.h"

class UBorder;
class UButton;
class UTextBlock;
class UWidget;
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

	/** 1px rim around panels, cards and slots (the HUD's panel outline). */
	extern const FLinearColor PanelOutline;

	/** Losing / enemy-team red, for DEFEAT and red-team labels. */
	extern const FLinearColor Red;

	UTextBlock* MakeText(UWidgetTree* Tree, FName Name, const FString& Str, int32 FontSize,
		FLinearColor Color, ETextJustify::Type Justify = ETextJustify::Center);

	/** Menu button using the shared normal/hover/pressed brushes, with a centered label. */
	UButton* MakeButton(UWidgetTree* Tree, FName Name, const FString& Label, int32 FontSize = 22);

	/** The label inside a button built by MakeButton; use it to retitle a button at runtime. */
	UTextBlock* GetButtonLabel(UButton* Btn);

	/**
	 * Solid rounded rect (optionally outlined) with no texture: the same brush
	 * MakeButton uses, for panels, bar tracks, badges and slot frames.
	 */
	FSlateBrush RoundedBrush(FLinearColor Fill, float Radius,
		FLinearColor Outline = FLinearColor::Transparent, float OutlineWidth = 0.0f);

	/**
	 * Restyles a text's font in place: Roboto's "Bold" face and/or a dark 1px
	 * outline (the HUD's headline treatment). OutlineColor only matters with bOutline.
	 */
	void SetFontStyle(UTextBlock* TB, bool bBold, bool bOutline,
		FLinearColor OutlineColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.9f));

	/** MakeText in the bold face with a 1px outline: titles, headline numbers, names. */
	UTextBlock* MakeTitle(UWidgetTree* Tree, FName Name, const FString& Str, int32 FontSize,
		FLinearColor Color, ETextJustify::Type Justify = ETextJustify::Center);

	/** A Border carrying RoundedBrush(Fill, Radius, Outline, OutlineWidth) with the given padding. */
	UBorder* MakePanel(UWidgetTree* Tree, FName Name, FLinearColor Fill, float Radius,
		FLinearColor Outline, float OutlineWidth, FMargin Padding);

	/**
	 * The HUD's coin glyph: a gold disc with a dark "G", Size px square. Roboto
	 * has no coin character, so it is drawn rather than typed.
	 */
	UWidget* MakeCoin(UWidgetTree* Tree, float Size = 18.0f);

	/** The HUD's ability key badge: a small rounded square holding Key. */
	UWidget* MakeKeyBadge(UWidgetTree* Tree, const FString& Key, float Size = 24.0f,
		FLinearColor Outline = PanelOutline);

	/**
	 * Marks a MakeButton button as the active choice (filter tab, selected tile):
	 * a 2px Accent outline on every state, or back to the stock 1px rim.
	 */
	void SetButtonHighlight(UButton* Btn, bool bOn, FLinearColor Accent = Gold);
}
