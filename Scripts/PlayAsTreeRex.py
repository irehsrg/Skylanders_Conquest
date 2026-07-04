import unreal
w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
gi = unreal.GameplayStatics.get_game_instance(w)
gi.set_editor_property('SelectedCharacterID', 'TreeRex')
unreal.SystemLibrary.execute_console_command(w, 'open Joust')
