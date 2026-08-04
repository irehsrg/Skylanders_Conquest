# Prints skeletal-mesh height and bottom-Z for characters, used to compute the
# per-model Scale / MeshZ entries in the C++ model tables.
#   set SKY_CHARS=Terrabite,Drobit  &&  UnrealEditor-Cmd ... -script=PrintMeshBounds.py
import os
import unreal

names = [c for c in os.environ.get('SKY_CHARS', '').split(',') if c.strip()]
for n in names:
    m = unreal.load_asset('/Game/Characters/{}/Models/{}'.format(n, n))
    if not m:
        unreal.log_warning('MISSING {}'.format(n))
        continue
    b = m.get_bounds()
    h = b.box_extent.z * 2
    bottom = b.origin.z - b.box_extent.z
    unreal.log('BOUNDS {} height={:.1f} bottomZ={:.1f}'.format(n, h, bottom))
