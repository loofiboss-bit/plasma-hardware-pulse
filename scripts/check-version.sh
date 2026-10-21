#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.."
python3 - <<'PY'
import json
import pathlib
import re
import xml.etree.ElementTree as ET
root = pathlib.Path('.')
version = (root / 'VERSION').read_text().strip()
assert re.fullmatch(r'\d+\.\d+\.\d+', version), 'VERSION must be MAJOR.MINOR.PATCH'
assert json.loads((root / 'package/metadata.json').read_text())['KPlugin']['Version'] == version, 'Widget version mismatch'
spec = (root / 'packaging/plasma-hardware-pulse.spec').read_text()
assert re.search(r'^Version:\s+' + re.escape(version) + r'\s*$', spec, re.M), 'RPM version mismatch'
latest = ET.parse(root / 'packaging/com.github.loofi.hardwarepulse.metainfo.xml').find('releases/release')
assert latest is not None and latest.attrib['version'] == version, 'AppStream version mismatch'
print(f'Version metadata aligned: {version}')
PY
