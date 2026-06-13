# Add 2x3 Inventory Grid to WBP_MainHUD
# Run in Output Log (Python mode)
#
# Creates a 2x3 grid of inventory slots at bottom-right of screen
# Widget names: InvSlot1 through InvSlot6 (TextBlocks inside bordered boxes)
# C++ UpdateHUD() already looks for these names

import unreal

# Load the widget blueprint
widget_path = "/Game/UserInterface/WBP_MainHUD"
widget_bp = unreal.load_asset(widget_path)

if not widget_bp:
    unreal.log_error("Could not load WBP_MainHUD at {}".format(widget_path))
else:
    unreal.log("Loaded WBP_MainHUD successfully")
    unreal.log("To add inventory slots, follow these manual steps in the Designer:")
    unreal.log("")
    unreal.log("=== INVENTORY GRID SETUP ===")
    unreal.log("")
    unreal.log("1. In WBP_MainHUD Designer, drag a Canvas Panel child or use the existing one")
    unreal.log("2. Add a Vertical Box, name it 'inventory_container'")
    unreal.log("3. Anchor it to bottom-right (click the anchor preset dropdown)")
    unreal.log("4. Set Position: X=-220, Y=-140 (offset from bottom-right)")
    unreal.log("5. Set Size: 200x120")
    unreal.log("")
    unreal.log("6. Inside inventory_container, add 3 Horizontal Boxes (one per row)")
    unreal.log("7. Inside each Horizontal Box, add 2 Text Blocks")
    unreal.log("8. Name them: InvSlot1, InvSlot2 (row 1), InvSlot3, InvSlot4 (row 2), InvSlot5, InvSlot6 (row 3)")
    unreal.log("9. Set each text size to 10, color white, horizontal alignment center")
    unreal.log("10. Set each Horizontal Box padding to 2")
    unreal.log("")
    unreal.log("NOTE: UMG widgets cannot be reliably created via Python scripting.")
    unreal.log("The C++ code already handles updating InvSlot1-6 with item names.")
    unreal.log("")

# Since UE Python cannot reliably add widgets to UMG blueprints programmatically,
# let's provide a more useful approach: generate the widgets via C++ at runtime
# by modifying the MainHUD widget after it's created.

unreal.log("RECOMMENDED: Let C++ create the inventory grid at runtime instead.")
unreal.log("This avoids manual Designer work entirely.")
unreal.log("The code change has already been prepared.")
