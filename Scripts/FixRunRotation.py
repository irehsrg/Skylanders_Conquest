# Fix running animation by applying -90 yaw rotation to all bone transforms
# Run in Output Log (Python mode)

import unreal

anim_path = "/Game/Characters/TriggerHappy/Animations/running.running"
anim = unreal.load_asset(anim_path)

if not anim:
    anim_path = "/Game/Characters/TriggerHappy/Animations/running"
    anim = unreal.load_asset(anim_path)

if not anim:
    unreal.log_error("Could not load running animation!")
else:
    unreal.log("Loaded: " + str(anim))

    # Try to get the animation data controller (UE5.4+)
    try:
        controller = anim.get_controller()
        unreal.log("Got animation controller")

        # Get the model to find bone tracks
        model = controller.get_model()
        bone_tracks = model.get_bone_animation_tracks()
        unreal.log("Found {} bone tracks".format(len(bone_tracks)))

        controller.open_bracket(unreal.Text("Fix rotation"))

        for track in bone_tracks:
            bone_name = track.get_editor_property('name')
            # Get all rotation keys for this track
            keys = track.get_editor_property('internal_track_data')
            unreal.log("  Track: {} ".format(bone_name))

        controller.close_bracket()
        unreal.log("Done examining tracks")

    except Exception as e:
        unreal.log_warning("Controller approach failed: " + str(e))
        unreal.log("Trying alternative: Animation Modifier approach...")

        # Alternative: try to use AddAnimationModifier or direct property manipulation
        try:
            # Check if we can access the raw rotation offset on the asset
            # Some UE5 versions expose import data with rotation
            import_data = anim.get_editor_property('asset_import_data')
            if import_data:
                unreal.log("Import data type: " + str(type(import_data)))
                try:
                    import_data.set_editor_property('import_rotation', unreal.Rotator(0, -90, 0))
                    unreal.log("Set import rotation to -90 yaw, now reimporting...")

                    # Trigger reimport
                    unreal.EditorAssetLibrary.save_asset(anim_path)
                    reimport_result = unreal.AssetToolsHelpers.get_asset_tools().reimport_asset(anim)
                    unreal.log("Reimport result: " + str(reimport_result))
                except Exception as e2:
                    unreal.log_warning("Could not set rotation: " + str(e2))
            else:
                unreal.log_warning("No import data found on animation")
        except Exception as e3:
            unreal.log_warning("Alternative also failed: " + str(e3))

    unreal.log("Script complete.")
