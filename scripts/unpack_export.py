#!/usr/bin/env python3
"""Reopen an exported investigation offline and verify every original byte."""
import hashlib
import json
from pathlib import Path
import re
import sys

if len(sys.argv) != 3:
    raise SystemExit('Uso: python3 scripts/unpack_export.py PACOTE.json NOVO_DIRETORIO')
source, destination = Path(sys.argv[1]), Path(sys.argv[2])
bundle = json.loads(source.read_bytes())
if bundle.get('format') != 'NOESE-INQUIRY-1':
    raise SystemExit('Formato de pacote não suportado')
if destination.exists():
    raise SystemExit('Destino deve ser um diretório novo')
originals = []
for original in bundle['originals']:
    if not re.fullmatch('[a-f0-9]{32}', original['artifact_id']):
        raise SystemExit('ID inválido')
    if original['encoding'] != 'hex':
        raise SystemExit('Codificação não suportada')
    content = bytes.fromhex(original['bytes'])
    if hashlib.sha256(content).hexdigest() != original['sha256']:
        raise SystemExit('Hash de original inválido')
    originals.append((original['artifact_id'], content))
destination.mkdir(mode=0o700, parents=True)
(destination / 'objects').mkdir(mode=0o700)
for identifier, content in originals:
    (destination / 'objects' / identifier).write_bytes(content)
(destination / 'inquiry.json').write_bytes(source.read_bytes())
print(f'{len(originals)} originais verificados; registros e versões em {destination / "inquiry.json"}')
print('Este utilitário reabre o pacote em arquivos. Não mescla investigações em um banco existente.')
