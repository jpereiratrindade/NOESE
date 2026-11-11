#!/usr/bin/env python3
"""Real local-model development run; LAB-001 is not an independent scientific evaluation."""
import argparse
import datetime
import hashlib
import json
from pathlib import Path
import socket
import subprocess
import time
import urllib.request
import uuid

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--model', default='qwen2.5:3b')
parser.add_argument('--timeout', type=int, default=1200)
args = parser.parse_args()
output = root / 'var/experiments' / ('semantic-' + uuid.uuid4().hex[:12])
output.mkdir(parents=True, mode=0o700)
with socket.socket() as sock:
    sock.bind(('127.0.0.1', 0))
    port = sock.getsockname()[1]
base = f'http://127.0.0.1:{port}/api/v1/'
log = open(output / 'server.log', 'w')
server = subprocess.Popen([str(root / 'build/noese'), '--data', str(output / 'archive'), '--port', str(port)], stdout=log, stderr=log)

def request(path, body=None):
    headers = {'X-NOESE-Token': token}
    if body is not None:
        headers['Content-Type'] = 'application/json'
        body = json.dumps(body).encode()
    with urllib.request.urlopen(urllib.request.Request(base + path, body, headers), timeout=10) as response:
        return json.load(response)

try:
    token = ''
    for _ in range(100):
        try:
            token = request('session')['token']
            break
        except OSError:
            time.sleep(.05)
    if not token:
        raise SystemExit('Server did not start')
    imported = request('lab/import?window=2', {})
    assert len(imported['artifacts']) == 21
    request('attention/settings', {'enabled': True, 'model': args.model, 'window': 2})
    start = time.monotonic()
    previous = None
    while time.monotonic() - start < args.timeout:
        status = request('attention?window=2')
        phase = status['state']['phase']
        if phase != previous:
            print(f'{round(time.monotonic()-start, 1)}s: {phase}', flush=True)
            previous = phase
        if status['run']:
            run = status['run']
            for finding in run['output']['findings']:
                for evidence in finding['evidence']:
                    with urllib.request.urlopen(base+'artifacts/'+evidence['artifact_id']+'/original') as response:
                        original = response.read()
                    fragment = original[evidence['start']:evidence['end']]
                    assert fragment.decode() == evidence['quote']
                    assert hashlib.sha256(fragment).hexdigest() == evidence['fragment_sha256']
            (output/'run.json').write_text(json.dumps(run, ensure_ascii=False, indent=2))
            summary = {'corpus':'LAB-001/PARTIAL_EXCERPTS','independent_evaluation':False,
                       'engine_generated':True,'model':args.model,'findings':len(run['output']['findings']),
                       'rejected':len(run['output']['rejected']), 'seconds':round(time.monotonic()-start,1),
                       'source_quotes_verified':True,'status':run['status'], 'evidence':str(output),
                       'recorded_at':datetime.datetime.now(datetime.timezone.utc).isoformat(),
                       'commit_before_run':subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip()}
            (output/'summary.json').write_text(json.dumps(summary, ensure_ascii=False, indent=2))
            print(json.dumps(summary, ensure_ascii=False, indent=2), flush=True)
            break
        if phase == 'unavailable':
            raise SystemExit(status['state'].get('message','Local capability unavailable'))
        time.sleep(3)
    else:
        raise SystemExit('Timed out; no report fabricated')
finally:
    server.terminate()
    server.wait(timeout=10)
    log.close()
