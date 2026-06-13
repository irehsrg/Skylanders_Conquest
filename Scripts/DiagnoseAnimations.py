# Diagnose TriggerHappy animations - check for T-poses, list durations
# Run in Output Log (Python mode)

import unreal

unreal.log("=" * 60)
unreal.log("ALL IMPORTED ANIMATIONS")
unreal.log("=" * 60)

anim_path = "/Game/Characters/TriggerHappy/Animations"
assets = unreal.EditorAssetLibrary.list_assets(anim_path, recursive=True, include_folder=False)

t_pose_suspects = []
good_anims = []

for asset_path in sorted(assets):
    anim = unreal.load_asset(asset_path)
    if anim and isinstance(anim, unreal.AnimSequence):
        name = asset_path.split("/")[-1].split(".")[0]
        length = anim.get_editor_property('sequence_length')
        rate = anim.get_editor_property('rate_scale')

        status = "OK"
        if length < 0.05:
            status = "** T-POSE / SINGLE FRAME **"
            t_pose_suspects.append(name)
        elif length < 0.3:
            status = "VERY SHORT"
        good_anims.append(name) if status == "OK" else None

        unreal.log("  {} | {:.3f}s | rate:{:.1f} | {}".format(
            name.ljust(45), length, rate, status))

unreal.log("")
unreal.log("=" * 60)
unreal.log("SUMMARY: {} total, {} good, {} t-pose suspects".format(
    len(good_anims) + len(t_pose_suspects), len(good_anims), len(t_pose_suspects)))
if t_pose_suspects:
    unreal.log("T-pose suspects: " + ", ".join(t_pose_suspects))
unreal.log("=" * 60)
