# Imports the generated placeholder SFX wavs into /Game/Audio/SFX.
#
# Headless:
#   UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="Scripts/ImportSFX.py" -unattended -nullrhi

import os
import unreal

SRC = os.environ.get(
    'SKY_SFX_DIR',
    'C:/Users/ajoin/AppData/Local/Temp/claude/'
    'C--Users-ajoin-Unreal-Engine-Skylanders-Conquest/'
    '8a13b565-bcc0-4f34-a523-2111fb0f8fcd/scratchpad/SFX')
DEST = '/Game/Audio/SFX'

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()

if not os.path.isdir(SRC):
    unreal.log_error('SFX source dir not found: {}'.format(SRC))
else:
    tasks = []
    for wav in sorted(f for f in os.listdir(SRC) if f.lower().endswith('.wav')):
        task = unreal.AssetImportTask()
        task.set_editor_property('automated', True)
        task.set_editor_property('destination_path', DEST)
        task.set_editor_property('filename', os.path.join(SRC, wav).replace('\\', '/'))
        task.set_editor_property('replace_existing', True)
        task.set_editor_property('save', True)
        tasks.append(task)

    asset_tools.import_asset_tasks(tasks)

    imported = []
    for t in tasks:
        imported.extend(str(p) for p in t.get_editor_property('imported_object_paths'))
    unreal.log('Imported {} sounds:'.format(len(imported)))
    for p in imported:
        unreal.log('  ' + p)
