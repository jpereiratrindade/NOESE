#!/usr/bin/env python3
"""Install and enable the local NOESE web service in the user's systemd manager."""
import argparse
import datetime
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--data', type=Path, default=root / 'var', help='Existing or new archive directory')
parser.add_argument('--port', type=int, default=8787)
parser.add_argument('--dry-run', action='store_true', help='Print the unit without installing it')
args = parser.parse_args()
if not 1024 <= args.port <= 65535:
    parser.error('--port must be 1024..65535')
executable = root / 'build/noese'
if not executable.is_file() or not os.access(executable, os.X_OK):
    parser.error('Build the application first with ./scripts/build.sh')
data = args.data.expanduser().resolve()

def quote(value, *, command=False):
    text = str(value)
    if any(ord(c) < 32 or ord(c) == 127 for c in text):
        raise SystemExit('Control characters are not supported in service paths')
    text = text.replace('\\', '\\\\').replace('"', '\\"').replace('%', '%%')
    if command:
        text = text.replace('$', '$$')
    return '"' + text + '"'

unit = (root / 'config/noese.service.in').read_text()
for key, value in {
    '@ROOT@': str(root).replace('%', '%%'),
    '@EXECUTABLE@': quote(executable, command=True),
    '@DATA@': quote(data, command=True),
    '@ENV_FILE@': str(data / 'mcp.env').replace('%', '%%').replace(' ', '\\x20'),
    '@PORT@': str(args.port),
}.items():
    unit = unit.replace(key, value)
if args.dry_run:
    print(unit, end='')
    raise SystemExit(0)
for command in ('systemctl', 'systemd-analyze', 'loginctl'):
    if not shutil.which(command):
        raise SystemExit(f'Required command unavailable: {command}')
subprocess.run(['systemctl', '--user', 'show-environment'], check=True, stdout=subprocess.DEVNULL)
# Verify the concrete unit before changing the user's configuration.
with tempfile.TemporaryDirectory(prefix='noese-service-') as directory:
    candidate = Path(directory) / 'noese.service'
    candidate.write_text(unit)
    subprocess.run(['systemd-analyze', '--user', 'verify', str(candidate)], check=True)
config = Path(os.environ.get('XDG_CONFIG_HOME', str(Path.home() / '.config')))
if not config.is_absolute():
    raise SystemExit('XDG_CONFIG_HOME must be absolute')
target = config / 'systemd/user/noese.service'
target.parent.mkdir(parents=True, exist_ok=True)
if target.exists() and target.read_text() != unit:
    suffix = datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
    shutil.copy2(target, target.with_name('noese.service.previous-' + suffix))
target.write_text(unit)
target.chmod(0o644)
subprocess.run(['systemctl', '--user', 'daemon-reload'], check=True)
# Restart also applies configuration changes when reinstalling an existing service.
subprocess.run(['systemctl', '--user', 'enable', 'noese.service'], check=True)
subprocess.run(['systemctl', '--user', 'restart', 'noese.service'], check=True)
subprocess.run(['systemctl', '--user', 'is-active', 'noese.service'], check=True)
print(f'Installed: {target}')
print(f'URL: http://127.0.0.1:{args.port}')
linger = subprocess.run(['loginctl', 'show-user', str(os.getuid()), '-p', 'Linger', '--value'],
                        capture_output=True, text=True, check=True).stdout.strip()
if linger != 'yes':
    print('For startup before login, enable lingering for your user with loginctl enable-linger.')
else:
    print('Linger=yes: enabled service can start at boot before login.')
