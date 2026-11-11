#!/usr/bin/env python3
"""Stage exactly the four shared files as private references, never execute them."""
import hashlib
import json
from pathlib import Path
import sys
import zipfile

root = Path(__file__).resolve().parents[1]
expected = {
    'NOESE-CAVEMAN-FULL-M1-v0.1.0.md': 'docs/reference',
    'NOESE-LAB-001-CONTRASTE-v0.1.0.md': 'docs/reference',
    'NOESE-LAB-001-corpus.jsonl': 'fixtures/lab001',
    'NOESE-LAB-001-manifest.json': 'fixtures/lab001',
}
if len(sys.argv) != 2:
    raise SystemExit('Uso: python3 scripts/import_lab.py CAMINHO_DO_ZIP')
with zipfile.ZipFile(sys.argv[1]) as archive:
    payloads = {}
    for name in expected:
        if archive.getinfo(name).file_size > 1024 * 1024:
            raise SystemExit('Documento excede o limite de 1 MiB')
        payloads[name] = archive.read(name)
    manifest = json.loads(payloads['NOESE-LAB-001-manifest.json'])
    corpus = payloads['NOESE-LAB-001-corpus.jsonl']
    if hashlib.sha256(corpus).hexdigest() != manifest['sha256']:
        raise SystemExit('Hash do corpus difere do manifesto')
    if len(corpus.splitlines()) != manifest['entries']:
        raise SystemExit('Número de excertos difere do manifesto')
    for name, destination in expected.items():
        target = root / destination / name
        if target.exists() and target.read_bytes() != payloads[name]:
            raise SystemExit(f'Referência existente difere; preservada: {target}')
    for name, destination in expected.items():
        target = root / destination / name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.parent.chmod(0o700)
        target.write_bytes(payloads[name])
        target.chmod(0o600)
        print(hashlib.sha256(payloads[name]).hexdigest(), target.relative_to(root))
print('Referências privadas preparadas. Importe o recorte desejado pela interface local.')
