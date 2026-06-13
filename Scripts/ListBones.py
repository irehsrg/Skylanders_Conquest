# List all bones in TriggerHappy skeleton
# Run in Output Log (Python mode)

import unreal

mesh = unreal.load_asset("/Game/Characters/TriggerHappy/Models/TriggerHappy")
if not mesh:
    unreal.log_error("Could not load TriggerHappy mesh")
else:
    # Get bone count using the skeletal mesh
    num_bones = mesh.get_num_bones() if hasattr(mesh, 'get_num_bones') else -1
    unreal.log("Bone count method: {}".format(num_bones))

    # Try to get names via skeleton
    skel = mesh.get_editor_property('skeleton')
    if skel:
        unreal.log("Skeleton: " + str(skel))

    # Try ref skeleton approach
    try:
        ref_skel = mesh.get_editor_property('ref_skeleton')
        if ref_skel:
            unreal.log("RefSkeleton: " + str(ref_skel))
    except:
        pass

    # Try getting bone names from an animation's tracks
    anim = unreal.load_asset("/Game/Characters/TriggerHappy/Animations/idle")
    if not anim:
        anim = unreal.load_asset("/Game/Characters/TriggerHappy/Animations/idel_standing")
    if anim:
        unreal.log("Using animation to find bone tracks...")
        try:
            model = anim.get_data_model()
            if model:
                tracks = model.get_bone_animation_tracks()
                unreal.log("Found {} bone tracks:".format(len(tracks)))
                for i, track in enumerate(tracks):
                    name = str(track.get_editor_property('name'))
                    unreal.log("  {}: {}".format(i, name))
        except Exception as e:
            unreal.log_warning("Data model approach failed: " + str(e))

    # Fallback: try bone names via physics asset
    phys = unreal.load_asset("/Game/Characters/TriggerHappy/Models/TriggerHappy_PhysicsAsset")
    if phys:
        unreal.log("Physics asset found, trying to get bone names from it...")
        try:
            bodies = phys.get_editor_property('skeletal_body_setups')
            if bodies:
                for body in bodies:
                    bone_name = body.get_editor_property('bone_name')
                    unreal.log("  Physics bone: {}".format(bone_name))
        except Exception as e:
            unreal.log_warning("Physics approach failed: " + str(e))

unreal.log("Done.")
