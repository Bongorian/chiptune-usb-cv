#!/usr/bin/env python3
"""Generate the shared editor field schema. Run from the repository root."""
import json
from pathlib import Path
fields=json.loads(Path('firmware/protocol/parameters.json').read_text())
Path('firmware/src/sound_fields.inc').write_text('/* Generated from protocol/parameters.json by tools/generate_editor.py. */\n'+''.join(f'FIELD({f["name"]}, {f["type"].upper()}, {f["min"]}, {f["max"]})\n' for f in fields))
Path('editor/schema.js').write_text('// Generated; see firmware/protocol/parameters.json.\nexport const fields = '+json.dumps(fields,indent=2)+';\n')
