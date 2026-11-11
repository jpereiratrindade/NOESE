#!/usr/bin/env python3
"""Subprocess MCP tests, including shared HTTP persistence and authority boundaries."""
import concurrent.futures
import fcntl
import json
from pathlib import Path
import select
import socket
import subprocess
import sys
import tempfile
import time
import unittest
import urllib.error
import urllib.request
import uuid

BINARY = str(Path(sys.argv.pop(1)).resolve())

def rid():
    return str(uuid.uuid4())

class Client:
    def __init__(self, data, writes=False, name='test-client', initialize=True):
        args=[BINARY,'mcp','--data',str(data)]
        if writes:
            args.append('--allow-writes')
        self.process=subprocess.Popen(args,stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
        self.ident=0
        if initialize:
            response=self.rpc('initialize',{'protocolVersion':'2025-11-25','capabilities':{},'clientInfo':{'name':name,'version':'1.0'}})
            assert response['result']['protocolVersion']=='2025-11-25',response
            self.notify('notifications/initialized')

    def send(self, message):
        self.process.stdin.write(json.dumps(message,ensure_ascii=False).encode()+b'\n')
        self.process.stdin.flush()

    def receive(self):
        if not select.select([self.process.stdout],[],[],8)[0]:
            raise AssertionError('No MCP response within 8 seconds')
        line=self.process.stdout.readline()
        if not line:
            raise AssertionError('MCP ended: '+self.process.stderr.read().decode())
        return json.loads(line)

    def rpc(self, method, params=None):
        self.ident+=1
        self.send({'jsonrpc':'2.0','id':self.ident,'method':method,'params':params or {}})
        response=self.receive()
        assert response['id']==self.ident,response
        return response

    def notify(self, method):
        self.send({'jsonrpc':'2.0','method':method})

    def call(self, name, args=None):
        return self.rpc('tools/call',{'name':name,'arguments':args or {}})

    def value(self, name, args=None):
        response=self.call(name,args)
        assert 'error' not in response,response
        result=response['result']
        assert not result['isError'],result
        return json.loads(result['content'][0]['text'])

    def close(self):
        self.process.stdin.close()
        self.process.wait(timeout=8)
        errors=self.process.stderr.read()
        self.process.stdout.close()
        self.process.stderr.close()
        assert self.process.returncode==0,errors

    def __enter__(self):
        return self

    def __exit__(self,*_):
        self.close()

class MCP(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory(prefix='noese-mcp-')
        self.data=Path(self.tmp.name)/'archive'
        self.http=None

    def tearDown(self):
        if self.http:
            self.http.terminate()
            self.http.wait(timeout=5)
        self.tmp.cleanup()

    def start_http(self, extra_args=None, env=None):
        with socket.socket() as sock:
            sock.bind(('127.0.0.1',0))
            port=sock.getsockname()[1]
        self.url=f'http://127.0.0.1:{port}'
        cmd=[BINARY,'--data',str(self.data),'--port',str(port)]
        if extra_args:
            cmd.extend(extra_args)
        self.http=subprocess.Popen(cmd,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,env=env)
        for _ in range(150):
            try:
                with urllib.request.urlopen(self.url+'/api/v1/session',timeout=.2) as r:
                    self.token=json.load(r)['token']
                return
            except OSError:
                time.sleep(.02)
        self.fail('HTTP server failed to start')

    def http_request(self,path,payload=None,method=None):
        body=json.dumps(payload).encode() if payload is not None else None
        request=urllib.request.Request(self.url+path,body,{'Content-Type':'application/json','X-NOESE-Token':self.token},method=method)
        try:
            response=urllib.request.urlopen(request,timeout=8)
        except urllib.error.HTTPError as e:
            response=e
        with response:
            return response.status,json.load(response)

    def inquiry(self,c,window=2):
        return c.value('create_inquiry',{'title':'MCP integration test','window':window,'request_id':rid()})

    def text(self,c,text='Questão de teste?',window=2):
        return c.value('import_text',{'filename':'source.txt','text':text,'window':window,'request_id':rid()})

    def tool_error(self,response,code):
        self.assertTrue(response['result']['isError'],response)
        self.assertEqual(json.loads(response['result']['content'][0]['text'])['code'],code)

    def test_lifecycle_and_notifications(self):
        with Client(self.data,initialize=False) as c:
            self.assertEqual(c.rpc('tools/list')['error']['code'],-32000)
            self.assertEqual(c.rpc('ping')['result'],{})
            self.assertEqual(c.rpc('initialize')['error']['code'],-32602)
            init=c.rpc('initialize',{'protocolVersion':'unsupported-version','capabilities':{},'clientInfo':{'name':'client','version':'1'}})
            self.assertEqual(init['result']['protocolVersion'],'2025-11-25')
            self.assertEqual(c.rpc('tools/list')['error']['code'],-32000)
            c.notify('notifications/initialized')
            c.notify('unknown-notification')
            self.assertIn('tools',c.rpc('tools/list')['result'])
            self.assertEqual(c.rpc('initialize')['error']['code'],-32600)
            self.assertEqual(c.rpc('unknown-method')['error']['code'],-32601)

    def test_read_only_catalog_and_write_denial(self):
        with Client(self.data) as c:
            tools=c.rpc('tools/list')['result']['tools']
            self.assertTrue(all(t['annotations']['readOnlyHint'] for t in tools))
            self.assertNotIn('propose_note',[t['name'] for t in tools])
            self.tool_error(c.call('create_inquiry',{'request_id':rid()}),'AUTHORITY_REQUIRED')
            self.assertEqual(c.value('inquiries')['items'],[])
            self.assertEqual(c.value('explore')['status'],'NO_GROUNDED_OPPORTUNITY')

    def test_contracts_and_bad_protocol_input(self):
        with Client(self.data) as c:
            for name,args in [('artifacts',{'limit':True}),('artifacts',{'limit':0}),('artifacts',{'window':3}),('artifacts',{'unknown':1}),('record',{'type':'not_a_table','id':'x'}),('search',{'query':123}),('missing',{})]:
                with self.subTest(name=name,args=args):
                    self.assertEqual(c.call(name,args)['error']['code'],-32602)
            c.send([])
            self.assertEqual(c.receive()['error']['code'],-32600)
            c.send({'jsonrpc':'2.0','id':True,'method':'ping'})
            self.assertEqual(c.receive()['error']['code'],-32600)
            c.process.stdin.write(b'{broken\n');c.process.stdin.flush()
            self.assertEqual(c.receive()['error']['code'],-32700)
            c.process.stdin.write(('['*70+'0'+']'*70+'\n').encode());c.process.stdin.flush()
            self.assertEqual(c.receive()['error']['code'],-32700)
            self.assertEqual(c.rpc('ping')['result'],{})

    def test_shared_http_origin_and_versions(self):
        self.start_http()
        with Client(self.data,True,name='declared-client') as c:
            inquiry=self.inquiry(c)
            source=self.text(c,'Uma questão preservada?')
            self.assertIsNone(source['attributed_author'])
            self.assertEqual(source['created_by'],'mcp:declared-client')
            anchor=c.value('create_anchor',{'artifact_id':source['id'],'start':0,'end':len('Uma questão preservada?'.encode()),'request_id':rid()})
            args={'inquiry_id':inquiry['id'],'text':'Interpretação candidata do cliente','basis':[{'type':'anchors','id':anchor['id']}],'request_id':rid()}
            note=c.value('propose_note',args)
            self.assertEqual(note['status'],'INFERRED_CANDIDATE')
            code,web_note=self.http_request('/api/v1/statements/'+note['id'])
            self.assertEqual(code,200)
            self.assertEqual(web_note,note)
            self.assertEqual(web_note['source_type'],'mcp_client')
            history=c.value('history',{'type':'statements','id':note['id']})
            self.assertEqual(history['events'][0]['actor'],'mcp:declared-client')
            self.assertEqual(history['events'][0]['detail']['source_ref'],'mcp:declared-client')
            revised=c.value('revise_statement',{'id':note['id'],'version':1,'text':'Versão revista explicitamente','revision_reason':'Pedido de edição no teste','request_id':rid()})
            self.assertEqual(revised['version'],2)
            self.tool_error(c.call('revise_statement',{'id':note['id'],'version':1,'text':'Stale','revision_reason':'Conflict test','request_id':rid()}),'CONFLICT')
            reference=c.value('reference',{'id':inquiry['id']})
            self.assertEqual(reference['statements']['items'][0]['text'],revised['text'])
            self.assertEqual(len(c.value('history',{'type':'statements','id':note['id']})['versions']),2)
            exported=c.value('export_inquiry',{'id':inquiry['id']})
            self.assertEqual(bytes.fromhex(exported['originals'][0]['bytes']),'Uma questão preservada?'.encode())
            self.assertEqual(c.value('verify')['technical_integrity'],'VERIFIED')
            tools=c.rpc('tools/list')['result']['tools']
            self.assertNotIn('authorize_contribution',[t['name'] for t in tools])
            self.assertNotIn('run',[t['name'] for t in tools])
            _,catalog=self.http_request('/api/v1/tools')
            self.assertEqual(catalog['tools'],tools)

    def test_idempotency_survives_restart_and_rejects_changes(self):
        args={'title':'Idempotent inquiry','request_id':rid()}
        with Client(self.data,True) as c:
            original=c.value('create_inquiry',args)
            self.assertEqual(original,c.value('create_inquiry',args))
            self.tool_error(c.call('create_inquiry',{**args,'title':'Changed'}),'CONFLICT')
        with Client(self.data,True) as c:
            self.assertEqual(original,c.value('create_inquiry',args))
            self.assertEqual(len(c.value('inquiries')['items']),1)
        with Client(self.data,True,name='another-client') as c:
            self.tool_error(c.call('create_inquiry',args),'CONFLICT')

    def test_windows_and_utf8_paging(self):
        with Client(self.data,True) as c:
            i=self.inquiry(c,0)
            future=self.text(c,'Future evidence?',2)
            self.tool_error(c.call('artifact',{'id':future['id'],'window':0}),'NOT_FOUND')
            self.tool_error(c.call('explore',{'artifact_id':future['id'],'window':0}),'NOT_FOUND')
            self.assertEqual(c.value('artifacts',{'window':0})['items'],[])
            source=self.text(c,'ação',0)
            first=c.value('artifact',{'id':source['id'],'max_bytes':2})
            self.assertEqual(first['content']['text'],'a')
            self.assertEqual(first['content']['next_offset'],1)
            second=c.value('artifact',{'id':source['id'],'offset':1,'max_bytes':2})
            self.assertEqual(second['content']['text'],'ç')
            self.assertEqual(c.call('artifact',{'id':source['id'],'offset':2})['error']['code'],-32602)
            note=c.value('propose_note',{'inquiry_id':i['id'],'text':'J0','basis_unresolved':True,'limitation':'Test scope','window':0,'request_id':rid()})
            c.value('revise_statement',{'id':note['id'],'version':1,'text':'J2','revision_reason':'Later knowledge','window':2,'request_id':rid()})
            self.assertEqual(c.value('record',{'type':'statements','id':note['id'],'window':0})['text'],'J0')
            self.assertNotIn('"text": "J2"',json.dumps(c.value('export_inquiry',{'id':i['id'],'window':0})))

    def test_mcp_and_http_concurrent_revision(self):
        self.start_http()
        with Client(self.data,True) as c:
            inquiry=self.inquiry(c)
            note=c.value('propose_note',{'inquiry_id':inquiry['id'],'text':'Initial','basis_unresolved':True,'limitation':'No source','request_id':rid()})
            with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
                rpc=pool.submit(c.call,'revise_statement',{'id':note['id'],'version':1,'text':'MCP version','revision_reason':'Concurrent','request_id':rid()})
                http=pool.submit(self.http_request,'/api/v1/statements/'+note['id'],{'version':1,'text':'HTTP version','revision_reason':'Concurrent'},'PUT')
                mcp_response=rpc.result();http_status,_=http.result()
            successes=int(not mcp_response['result']['isError'])+int(http_status==200)
            self.assertEqual(successes,1)
            self.assertEqual(len(c.value('history',{'type':'statements','id':note['id']})['versions']),2)

    def test_process_lock_covers_http_and_mcp(self):
        self.start_http()
        with Client(self.data) as c:
            with open(self.data/'.archive.lock','rb') as lock:
                fcntl.flock(lock,fcntl.LOCK_EX)
                with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
                    rpc=pool.submit(c.value,'inquiries')
                    http=pool.submit(self.http_request,'/health')
                    try:
                        done,_=concurrent.futures.wait([rpc,http],timeout=.1)
                        self.assertFalse(done)
                    finally:
                        fcntl.flock(lock,fcntl.LOCK_UN)
                    self.assertEqual(rpc.result()['items'],[])
                    self.assertEqual(http.result()[0],200)

    def test_sensitive_erasure_revokes_cached_retries(self):
        self.start_http()
        secret='private-mcp-erasure-token'
        with Client(self.data,True) as c:
            i=self.inquiry(c)
            args={'filename':'private.txt','text':secret,'request_id':rid()}
            a=c.value('import_text',args)
            anchor=c.value('create_anchor',{'artifact_id':a['id'],'start':0,'end':len(secret),'request_id':rid()})
            note_args={'inquiry_id':i['id'],'text':secret,'basis':[{'type':'anchors','id':anchor['id']}],'request_id':rid()}
            c.value('propose_note',note_args)
            code,_=self.http_request('/api/v1/artifacts/'+a['id'],{'confirm_sha256':a['sha256']},'DELETE')
            self.assertEqual(code,200)
            self.tool_error(c.call('import_text',args),'SOURCE_ERASED')
            self.tool_error(c.call('propose_note',note_args),'SOURCE_ERASED')
            self.assertEqual(c.value('artifacts')['items'],[])
            self.assertNotIn(secret.encode(),(self.data/'noese.sqlite').read_bytes())

    def test_proposals_and_nested_reference_validation(self):
        with Client(self.data,True) as c:
            i=self.inquiry(c)
            note=c.value('propose_note',{'inquiry_id':i['id'],'text':'A question?','basis_unresolved':True,'limitation':'Not sourced','request_id':rid()})
            refs=[{'type':'inquiries','id':i['id']},{'type':'statements','id':note['id']}]
            r=c.value('propose_relation',{'inquiry_id':i['id'],'text':'Candidate relation','references':refs,'rationale':'Explicit test','basis':refs,'request_id':rid()})
            self.assertEqual(r['status'],'INFERRED_CANDIDATE')
            o=c.value('propose_opportunity',{'inquiry_id':i['id'],'text':'Consider comparison','basis':refs,'request_id':rid()})
            self.assertEqual(o['execution'],'NOT_PERFORMED')
            self.assertEqual(c.call('propose_note',{'inquiry_id':i['id'],'text':'Forged actor','created_by':'human','request_id':rid()})['error']['code'],-32602)
            self.assertEqual(c.call('propose_note',{'inquiry_id':i['id'],'text':'Invalid ref','basis':[{'type':'statements','id':note['id'],'extra':True}],'request_id':rid()})['error']['code'],-32602)
            c.process.stdin.write(b'\n');c.process.stdin.flush()
            self.assertEqual(c.receive()['error']['code'],-32700)

    def mcp_http(self,payload=None,path='/mcp',method='POST',headers=None):
        body=json.dumps(payload).encode() if payload is not None else None
        req_headers={'Content-Type':'application/json'}
        if headers:
            req_headers.update(headers)
        request=urllib.request.Request(self.url+path,body,req_headers,method=method)
        try:
            response=urllib.request.urlopen(request,timeout=8)
        except urllib.error.HTTPError as e:
            response=e
        with response:
            raw=response.read()
            return response.status,dict(response.headers),json.loads(raw) if raw else None

    def test_bounded_input_and_stdio_only(self):
        result=subprocess.run([BINARY,'mcp','--data',str(self.data)],input=b'x'*(1024*1024+1)+b'\n',capture_output=True,timeout=8)
        self.assertNotEqual(result.returncode,0)
        self.assertEqual(result.stdout,b'')
        self.assertIn(b'1 MiB',result.stderr)
        invalid=subprocess.run([BINARY,'--allow-writes','--data',str(self.data)],capture_output=True,timeout=8)
        self.assertNotEqual(invalid.returncode,0)
        self.assertEqual(invalid.stdout,b'')

    def test_streamable_http_mcp_auth_and_shared_writes(self):
        secret='test-mcp-secret-token'
        self.start_http(['--mcp-allow-writes','--mcp-token',secret])
        status,_,err=self.mcp_http({'jsonrpc':'2.0','id':1,'method':'tools/list'})
        self.assertEqual(status,401)
        self.assertEqual(err['code'],'AUTHORITY_REQUIRED')
        status,_,err=self.mcp_http({'jsonrpc':'2.0','id':1,'method':'tools/list'},headers={'Authorization':'Bearer wrong'})
        self.assertEqual(status,401)
        # External Host is still blocked on web/API routes even when MCP token is set.
        status,_,_=self.mcp_http(None,path='/api/v1/session',method='GET',headers={'Host':'tunnel.example.com'})
        self.assertEqual(status,403)
        status,hdrs,_=self.mcp_http(None,path='/mcp',method='OPTIONS')
        self.assertEqual(status,204)
        self.assertIn('POST',hdrs.get('Allow',''))
        status,hdrs,_=self.mcp_http(None,path='/mcp',method='GET')
        self.assertEqual(status,405)
        # Tunnel Host works on /mcp with Bearer token.
        auth={'Authorization':'Bearer '+secret,'Host':'tunnel.example.com'}
        status,hdrs,init=self.mcp_http({'jsonrpc':'2.0','id':1,'method':'initialize','params':{'protocolVersion':'2025-06-18','capabilities':{},'clientInfo':{'name':'chatgpt-tunnel','version':'1.0'}}},headers=auth)
        self.assertEqual(status,200)
        self.assertEqual(init['result']['protocolVersion'],'2025-06-18')
        sid=hdrs.get('Mcp-Session-Id')
        self.assertTrue(sid)
        status,_,empty=self.mcp_http({'jsonrpc':'2.0','method':'notifications/initialized'},headers={**auth,'Mcp-Session-Id':sid})
        self.assertEqual(status,202)
        self.assertIsNone(empty)
        status,_,listed=self.mcp_http({'jsonrpc':'2.0','id':2,'method':'tools/list'},headers={**auth,'Mcp-Session-Id':sid})
        self.assertEqual(status,200)
        names={t['name'] for t in listed['result']['tools']}
        self.assertIn('propose_note',names)
        # Write via HTTP /mcp and verify via web API and path-token endpoint.
        status,_,created=self.mcp_http({'jsonrpc':'2.0','id':3,'method':'tools/call','params':{'name':'create_inquiry','arguments':{'title':'HTTP MCP','request_id':rid()}}},headers={**auth,'Mcp-Session-Id':sid})
        self.assertEqual(status,200)
        inq=json.loads(created['result']['content'][0]['text'])
        self.assertEqual(inq['created_by'],'mcp:chatgpt-tunnel')
        status,_,listed_path=self.mcp_http({'jsonrpc':'2.0','id':4,'method':'tools/call','params':{'name':'inquiries','arguments':{'limit':None,'offset':None,'window':None}}},path='/mcp/'+secret,headers={'Host':'tunnel.example.com'})
        self.assertEqual(status,200)
        items=json.loads(listed_path['result']['content'][0]['text'])['items']
        self.assertEqual(len(items),1)
        self.assertEqual(items[0]['id'],inq['id'])
        status,_,listed_null=self.mcp_http({'jsonrpc':'2.0','id':5,'method':'tools/call','params':{'name':'inquiries','arguments':None}},path='/mcp/'+secret,headers={'Host':'tunnel.example.com'})
        self.assertEqual(status,200)
        self.assertEqual(len(json.loads(listed_null['result']['content'][0]['text'])['items']),1)
        status,_,_=self.mcp_http(None,path='/mcp',method='DELETE',headers=auth)
        self.assertEqual(status,204)

    def test_streamable_http_mcp_readonly_by_default(self):
        self.start_http()
        status,_,listed=self.mcp_http({'jsonrpc':'2.0','id':1,'method':'tools/list'})
        self.assertEqual(status,200)
        names={t['name'] for t in listed['result']['tools']}
        self.assertNotIn('propose_note',names)
        status,_,res=self.mcp_http({'jsonrpc':'2.0','id':2,'method':'tools/call','params':{'name':'create_inquiry','arguments':{'title':'Blocked','request_id':rid()}}})
        self.assertEqual(status,200)
        self.assertTrue(res['result']['isError'])

if __name__=='__main__':
    unittest.main(verbosity=2)
