#!/usr/bin/env python3
"""Deterministic provider fixtures test contracts, not scientific usefulness or model quality."""
import http.server
import json
import os
from pathlib import Path
import select
import socket
import subprocess
import sys
import tempfile
import threading
import time
import unittest
import urllib.error
import urllib.request
import uuid

BINARY = str(Path(sys.argv.pop(1)).resolve())

class InvestigatorTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory(prefix='noese-investigator-')
        self.data=Path(self.tmp.name)/'archive'
        self.mode='normal'
        self.calls=[]
        self.entered=threading.Event()
        self.release=threading.Event()
        self.server=None
        owner=self
        class Provider(http.server.BaseHTTPRequestHandler):
            def log_message(self,*args):
                pass
            def do_GET(self):
                self.send_response(200);self.send_header('Content-Type','application/json');self.end_headers()
                self.wfile.write(json.dumps({'models':[{'name':'contract-fixture','digest':'contract-test-only'}]}).encode())
            def do_POST(self):
                body=json.loads(self.rfile.read(int(self.headers['Content-Length'])))
                prompt=json.loads(body['messages'][1]['content'])
                owner.calls.append(body)
                if 'candidate_findings' in prompt:
                    value={'reviews':[{'index':i,'keep':owner.mode!='reject','reason':'Contract fixture counterpoint, not independent evaluation.','alternative':'Another interpretation remains possible.'} for i,_ in enumerate(prompt['candidate_findings'])],'limits':[]}
                else:
                    if owner.mode=='block':
                        owner.entered.set();owner.release.wait(15)
                    evidence=[{'source':a['source'],'quote':a['text']} for a in prompt['sources'][:2]]
                    if owner.mode=='fabrication':evidence[0]['quote']='This literal quotation does not occur in any source.'
                    if owner.mode=='duplicate':evidence=[evidence[0],evidence[0]]
                    finding={'title':'Contract test interpretation','interpretation':'The two notes support a candidate tension.','why_attention':'There are different constraints to confront.','alternatives':['The constraints may refer to different situations.'],'evidence':evidence,'contributions':[{'proposal':'Seek an independent perspective.','reason':'The context is incomplete.','alternative':'Keep the question open.','capability_id':'unavailable','authorization_needed':'Requires explicit scope and a real external source.'}]}
                    value={'findings':[] if owner.mode=='empty' else [finding],'limits':['Synthetic provider fixture; no scientific claim.']}
                response={'model':'contract-fixture','message':{'content':json.dumps(value)},'done':True,'done_reason':'length' if owner.mode=='truncated' else 'stop'}
                if owner.mode=='bad-json':response['message']['content']='not-json'
                payload=json.dumps(response).encode()
                self.send_response(200);self.send_header('Content-Type','application/json');self.end_headers();self.wfile.write(payload)
        self.provider=http.server.ThreadingHTTPServer(('127.0.0.1',0),Provider)
        self.provider_thread=threading.Thread(target=self.provider.serve_forever,daemon=True);self.provider_thread.start()
        self.first='Initial note: allocation is twelve units, without operational costs.'
        self.second='Later note: the available allocation is six units after operational costs.'
        self.artifacts=self.seed([self.first,self.second])

    def tearDown(self):
        self.release.set()
        if self.server:self.server.terminate();self.server.wait(timeout=8)
        self.provider.shutdown();self.provider.server_close();self.provider_thread.join(timeout=3)
        self.tmp.cleanup()

    def seed(self,texts,window=2,role="source"):
        messages=[{'jsonrpc':'2.0','id':1,'method':'initialize','params':{'protocolVersion':'2025-11-25','capabilities':{},'clientInfo':{'name':'contract-setup','version':'1'}}},{'jsonrpc':'2.0','method':'notifications/initialized'}]
        for index,text in enumerate(texts):messages.append({'jsonrpc':'2.0','id':index+2,'method':'tools/call','params':{'name':'import_text','arguments':{'filename':uuid.uuid4().hex+'.txt','text':text,'investigative_role':role,'window':window,'request_id':str(uuid.uuid4())}}})
        result=subprocess.run([BINARY,'mcp','--data',str(self.data),'--allow-writes'],input=''.join(json.dumps(m)+'\n' for m in messages),text=True,capture_output=True,check=True,timeout=10)
        responses=[json.loads(line) for line in result.stdout.splitlines()]
        return [json.loads(r['result']['content'][0]['text']) for r in responses[1:]]

    def config(self,enabled=True,window=2):
        (self.data/'investigator.json').write_text(json.dumps({'enabled':enabled,'model':'contract-fixture','port':self.provider.server_port,'window':window}))

    def once(self,window=2,success=True):
        result=subprocess.run([BINARY,'investigate','--data',str(self.data),'--window',str(window)],capture_output=True,text=True,timeout=20)
        if success:
            self.assertEqual(result.returncode,0,result.stderr)
            return json.loads(result.stdout)
        self.assertNotEqual(result.returncode,0)
        return result

    def start_http(self):
        with socket.socket() as sock:sock.bind(('127.0.0.1',0));port=sock.getsockname()[1]
        self.url=f'http://127.0.0.1:{port}'
        self.server=subprocess.Popen([BINARY,'--data',str(self.data),'--port',str(port)],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
        for _ in range(100):
            try:
                self.token=self.request('session')['token'];return
            except OSError:time.sleep(.03)
        self.fail('Server not ready')

    def request(self,path,body=None,method=None,expected=200):
        payload=json.dumps(body).encode() if body is not None else None
        req=urllib.request.Request(self.url+'/api/v1/'+path,payload,{'Content-Type':'application/json','X-NOESE-Token':getattr(self,'token','')},method=method)
        try:response=urllib.request.urlopen(req,timeout=8)
        except urllib.error.HTTPError as e:response=e
        with response:
            data=json.load(response);self.assertEqual(response.status,expected,data);return data

    def wait_report(self):
        for _ in range(300):
            value=self.request('attention')
            if value['run']:return value
            self.assertNotEqual(value['state']['phase'],'unavailable',value)
            time.sleep(.03)
        self.fail('No automatic report')

    def test_semantic_contract_and_grounding_without_question(self):
        self.config();run=self.once()
        self.assertNotIn('?',self.first+self.second)
        self.assertEqual(run['capability_id'],'semantic-exploration')
        self.assertEqual(run['status'],'CANDIDATES')
        finding=run['output']['findings'][0]
        self.assertEqual(len(finding['evidence']),2)
        self.assertFalse(finding['automated_counterpoint']['independent'])
        self.assertEqual(finding['contributions'][0]['execution'],'NOT_PERFORMED')
        self.assertEqual(finding['contributions'][0]['availability'],'CAPABILITY_UNAVAILABLE')
        for e in finding['evidence']:
            a=next(a for a in self.artifacts if a['id']==e['artifact_id'])
            original=(self.data/'objects'/a['sha256']).read_bytes()
            self.assertEqual(original[e['start']:e['end']].decode(),e['quote'])
            self.assertIn('anchor_id',e)
        self.assertEqual(len(self.calls),2)
        self.assertEqual(self.calls[0]['options']['num_gpu'],0)
        prompt=json.loads(self.calls[0]['messages'][1]['content'])
        self.assertNotIn('question',prompt)
        self.assertNotIn('tools',self.calls[0])

    def test_technical_diary_never_consumes_context_or_restarts_inference(self):
        self.config(); first = self.once()
        self.seed(['Diário Pelotas evento instrumental'] * 35, role='instrumental_event')
        second = self.once()
        self.assertEqual(first['id'], second['id'])
        self.assertEqual(len(self.calls), 2)
        prompt = json.loads(self.calls[0]['messages'][1]['content'])
        self.assertEqual(len(prompt['sources']), 2)

    def test_missing_literal_citation_discards_finding(self):
        self.mode='fabrication';self.config();run=self.once()
        self.assertEqual(run['status'],'NO_GROUNDED_OPPORTUNITY')
        self.assertEqual(run['output']['findings'],[])
        self.assertTrue(run['output']['rejected'])
        self.assertEqual(len(self.calls),1)

    def test_duplicate_evidence_is_not_two_supports(self):
        self.mode='duplicate';self.config();run=self.once()
        self.assertEqual(run['output']['findings'],[])

    def test_counterpoint_can_reject_every_candidate(self):
        self.mode='reject';self.config();run=self.once()
        self.assertEqual(run['output']['findings'],[])
        self.assertEqual(run['output']['rejected'][0]['stage'],'counterpoint')

    def test_empty_result_does_not_create_empty_inquiry(self):
        self.mode='empty';self.config();run=self.once()
        self.assertIsNone(run['inquiry_id'])
        self.assertEqual(run['status'],'NO_GROUNDED_OPPORTUNITY')
        self.assertEqual(len(self.calls),1)

    def test_bad_or_truncated_model_output_is_not_published(self):
        self.config()
        for mode in ['bad-json','truncated']:
            self.mode=mode
            self.once(success=False)
        self.config(False);self.start_http()
        self.assertIsNone(self.request('attention')['run'])

    def test_continuity_uses_snapshot_cache_after_restart(self):
        self.config();first=self.once();second=self.once()
        self.assertEqual(first,second)
        self.assertEqual(len(self.calls),2)

    def test_future_source_is_not_sent_to_earlier_window(self):
        self.seed(['A source available only in the early recorte.'],window=0)
        self.config(window=0)
        self.once(window=0)
        prompt=json.loads(self.calls[0]['messages'][1]['content'])
        self.assertEqual(len(prompt['sources']),1)
        self.assertNotIn(self.first,json.dumps(prompt))

    def test_archive_change_during_inference_discards_stale_result(self):
        self.mode='block';self.config()
        process=subprocess.Popen([BINARY,'investigate','--data',str(self.data)],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
        try:
            self.assertTrue(self.entered.wait(5))
            self.seed(['New authorized source added during inference.'])
            self.release.set();out,err=process.communicate(timeout=15)
            self.assertNotEqual(process.returncode,0)
            self.assertIn('mudou',err)
            self.assertEqual(out,'')
        finally:
            if process.poll() is None:process.kill();process.wait()

    def test_observer_runs_automatically_and_criticism_triggers_new_cycle(self):
        self.config(False);self.start_http()
        self.assertIsNone(self.request('attention')['run'])
        self.request('attention/settings',{'enabled':True})
        first=self.wait_report()['run']
        self.request('attention/'+first['id']+'/critique',{'text':'These may be different contexts; reconsider the causal wording.'},expected=201)
        for _ in range(300):
            current=self.request('attention')['run']
            if current and current['id']!=first['id']:break
            time.sleep(.03)
        else:self.fail('Feedback was not reconsidered')
        self.assertEqual(current['output']['previous_run_id'],first['id'])
        self.assertTrue(current['output']['human_criticism_considered'])
        self.assertTrue(json.loads(self.calls[-2]['messages'][1]['content'])['human_criticism'])
        self.assertTrue(all(c['execution']=='NOT_PERFORMED' for f in current['output']['findings'] for c in f['contributions']))

    def test_sensitive_erasure_removes_derived_semantic_report(self):
        self.config();run=self.once();self.config(False);self.start_http()
        source=self.artifacts[0]
        self.request('artifacts/'+source['id'],{'confirm_sha256':source['sha256']},method='DELETE')
        self.assertIsNone(self.request('attention')['run'])
        self.assertNotIn(self.first.encode(),(self.data/'noese.sqlite').read_bytes())

if __name__=='__main__':
    unittest.main(verbosity=2)
