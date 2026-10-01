"""Fail closed if the audited Newlib internal descriptor ABI may have changed."""
from pathlib import Path
import hashlib,json,subprocess
root=Path(__file__).resolve().parents[2]
meta=json.loads((root/'port/vita/vendor/newlib-descriptor.json').read_text())
header=root/'port/vita/vendor/vitadescriptor.h'
if hashlib.sha256(header.read_bytes()).hexdigest()!=meta['header_sha256']:
    raise SystemExit('Descriptor header changed; re-audit required')
for name,digest in meta['archive_members'].items():
    data=subprocess.check_output(['/home/birchwoodgod/vitasdk/bin/arm-vita-eabi-ar','p',
        '/home/birchwoodgod/vitasdk/arm-vita-eabi/lib/libc.a',name])
    if hashlib.sha256(data).hexdigest()!=digest:
        raise SystemExit('Newlib descriptor ABI unverified: '+name+' changed; re-audit required')
