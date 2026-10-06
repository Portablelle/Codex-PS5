#!/usr/bin/env python3
"""Check the RPC contract with a host Codex executable, without signing in."""
import json,os,selectors,subprocess,tempfile,time
with tempfile.TemporaryDirectory(prefix='codex-ps5-rpc-') as home:
 env={**os.environ,'CODEX_HOME':home}
 p=subprocess.Popen(['codex','app-server','--listen','stdio://'],stdin=subprocess.PIPE,
                    stdout=subprocess.PIPE,stderr=subprocess.DEVNULL,text=True,env=env)
 selector=selectors.DefaultSelector();selector.register(p.stdout,selectors.EVENT_READ)
 def send(value):p.stdin.write(json.dumps(value)+'\n');p.stdin.flush()
 def response(identifier):
  deadline=time.monotonic()+20
  while time.monotonic()<deadline:
   if not selector.select(max(0,deadline-time.monotonic())):break
   line=p.stdout.readline()
   if not line:raise RuntimeError('App-server exited')
   value=json.loads(line)
   if value.get('id')==identifier:return value
  raise RuntimeError('App-server response timeout')
 try:
  send({'id':1,'method':'initialize','params':{'clientInfo':{'name':'codex_ps5','title':'Codex PS5','version':'0.1.0'},'capabilities':{'experimentalApi':True}}})
  assert 'result' in response(1)
  send({'method':'initialized'})
  send({'id':2,'method':'account/read','params':{'refreshToken':False}})
  result=response(2);assert 'result' in result and result['result']['account'] is None
  send({'id':3,'method':'thread/start','params':{'cwd':home,'approvalPolicy':'untrusted','sandbox':'danger-full-access','developerInstructions':'PS5 prototype protocol test; no model turn will be started.'}})
  result=response(3)
  assert 'result' in result,result
  assert result['result']['thread']['id']
  send({'id':4,'method':'thread/start','params':{'cwd':home,'approvalPolicy':'never','sandbox':'read-only','developerInstructions':'PS5 gameplay companion contract test; no model turn will be started.'}})
  result=response(4)
  assert 'result' in result,result
  assert result['result']['thread']['id']
  # The app resumes its saved thread and starts a new one when Codex refuses it.
  send({'id':5,'method':'thread/resume','params':{'threadId':'00000000-0000-0000-0000-000000000000','cwd':home,'approvalPolicy':'untrusted','sandbox':'danger-full-access'}})
  result=response(5)
  assert 'error' in result,result
  print('Codex app-server: initialize, account/read, normal and gameplay thread/start accepted, unknown thread/resume refused; no prompt sent')
 finally:
  p.terminate()
  try:p.wait(timeout=5)
  except subprocess.TimeoutExpired:p.kill();p.wait()
