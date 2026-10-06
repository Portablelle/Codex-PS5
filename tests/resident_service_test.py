#!/usr/bin/env python3
"""Real loopback transports with fake engines; no console or OpenAI requests."""
import json
import os
from pathlib import Path
import queue
import shutil
import signal
import socket
import subprocess
import threading
import time

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build/resident-tests'
BUILD.mkdir(parents=True, exist_ok=True)


def free_port():
    with socket.socket() as sock:
        sock.bind(('127.0.0.1', 0))
        return sock.getsockname()[1]


def wait_for(check, timeout=5):
    end = time.monotonic() + timeout
    while time.monotonic() < end:
        value = check()
        if value:
            return value
        time.sleep(0.02)
    raise AssertionError('Timed out waiting for test condition')


def connect(port):
    def attempt():
        try:
            sock = socket.create_connection(('127.0.0.1', port), timeout=0.2)
            sock.settimeout(3)
            return sock
        except OSError:
            return None
    return wait_for(attempt)


class Peer:
    def __init__(self, port):
        self.sock = connect(port)
        self.buffer = b''
        self.auto_ack = True

    def send(self, message):
        self.sock.sendall(json.dumps(message, ensure_ascii=False).encode() + b'\n')

    def line(self):
        while b'\n' not in self.buffer:
            part = self.sock.recv(8192)
            if not part:
                raise EOFError('Peer disconnected')
            self.buffer += part
        line, self.buffer = self.buffer.split(b'\n', 1)
        message = json.loads(line)
        if self.auto_ack and 'action' in message and 'sequence' in message:
            self.send({'command': 'ack', 'instance': message['instance'], 'sequence': message['sequence']})
        return message

    def until(self, predicate):
        for _ in range(100):
            message = self.line()
            if predicate(message):
                return message
        raise AssertionError('Too many unrelated messages')

    def action(self, name):
        return self.until(lambda message: message.get('action') == name)

    def close(self):
        self.sock.close()


class FakeEngine:
    def __init__(self):
        self.listener = socket.socket()
        self.listener.bind(('127.0.0.1', 0))
        self.listener.listen(4)
        self.listener.settimeout(0.1)
        self.port = self.listener.getsockname()[1]
        self.clients = []
        self.messages = queue.Queue()
        self.errors = queue.Queue()
        self.done = threading.Event()
        self.thread = threading.Thread(target=self.run, daemon=True)
        self.thread.start()

    def run(self):
        try:
            while not self.done.is_set():
                try:
                    sock, _ = self.listener.accept()
                except socket.timeout:
                    continue
                self.clients.append(sock)
                threading.Thread(target=self.client, args=(sock,), daemon=True).start()
        except OSError:
            if not self.done.is_set():
                self.errors.put('Fake engine listener failed')

    def send(self, message):
        self.clients[-1].sendall(json.dumps(message).encode() + b'\n')

    def client(self, sock):
        try:
            with sock.makefile('rb') as stream:
                for line in stream:
                    message = json.loads(line)
                    self.messages.put(message)
                    method = message.get('method')
                    result = None
                    if method == 'initialize':
                        result = {}
                    elif method == 'account/read':
                        result = {'account': {'type': 'chatgpt'}}
                    elif method == 'thread/start':
                        assert message['params']['sandbox'] == 'read-only'
                        result = {'thread': {'id': 'game-thread'}}
                    elif method == 'turn/start':
                        result = {'turn': {'id': 'game-turn'}}
                    elif method == 'turn/interrupt':
                        result = {}
                    if result is not None:
                        sock.sendall(json.dumps({'id': message['id'], 'result': result}).encode() + b'\n')
        except (OSError, ValueError, AssertionError) as error:
            if not self.done.is_set():
                self.errors.put(str(error))

    def received(self, method):
        end = time.monotonic() + 3
        while time.monotonic() < end:
            message = self.messages.get(timeout=3)
            if message.get('method') == method:
                return message
        raise AssertionError('Expected RPC method not received')

    def close(self):
        self.done.set()
        self.listener.close()
        for sock in self.clients:
            try:
                sock.shutdown(socket.SHUT_RDWR)
            except OSError:
                pass
            sock.close()
        self.thread.join(timeout=2)


def resident_test():
    engine = FakeEngine()
    port = free_port()
    process = subprocess.Popen([str(ROOT / 'build/ingame-service'), '--adapter-port', str(port),
        '--engine-port', str(engine.port), '--capture-directory', str(BUILD)])
    peers = []
    try:
        observer = Peer(port); peers.append(observer)
        observer.send({'command': 'hello', 'role': 'observer'})
        state = observer.until(lambda message: message.get('event') == 'status' and message['ready'])
        instance = state['instance']
        cli = subprocess.run([shutil.which('python3'), str(ROOT / 'tools/ingame-control.py'),
            'status', '--port', str(port)], capture_output=True, text=True, check=True)
        assert json.loads(cli.stdout)['instance'] == instance
        adapter = Peer(port); peers.append(adapter)
        adapter.send({'command': 'hello', 'role': 'adapter'})
        adapter.until(lambda message: message.get('event') == 'status')

        def command(name, **extra):
            adapter.send({'command': name, 'instance': instance, **extra})

        command('capabilities', capabilities=dict.fromkeys(('capture', 'overlay', 'microphone', 'speech'), True))
        command('trigger')
        capture = adapter.action('capture')
        request = capture['request']
        assert capture['instance'] == instance
        assert f'/{instance}/ingame-{request}.png' in capture['path']
        command('adapter', message={'event': 'captured', 'request': request})
        assert adapter.action('overlay')['visible']
        adapter.action('record')
        command('trigger'); adapter.action('stopRecording')
        assert not adapter.action('overlay')['visible']
        command('adapter', message={'event': 'transcribed', 'request': request, 'text': 'Comment battre ce monstre ?'})
        turn = engine.received('turn/start')
        assert turn['params']['input'][1] == {'type': 'localImage', 'path': capture['path']}
        assert turn['params']['input'][0]['text'] == 'Comment battre ce monstre ?'
        observer.close()  # The interface disappears while inference continues.
        engine.send({'method': 'item/completed', 'params': {'item': {
            'type': 'agentMessage', 'id': 'answer', 'text': 'Esquive à gauche, puis attaque.'}}})
        engine.send({'method': 'turn/completed', 'params': {'turn': {'status': 'completed'}}})
        # Receiving bytes alone is not an acknowledgement of file cleanup.
        adapter.auto_ack = False
        unacknowledged = adapter.action('releaseCapture')
        adapter.auto_ack = True
        assert adapter.action('speak')['text'] == 'Esquive à gauche, puis attaque.'
        busy_stop = subprocess.run([shutil.which('python3'), str(ROOT / 'tools/ingame-control.py'),
            'stop', '--port', str(port)], capture_output=True, text=True)
        assert busy_stop.returncode == 1 and 'cleanup' in busy_stop.stderr
        replacement = Peer(port); peers.append(replacement)
        replacement.send({'command': 'hello', 'role': 'observer'})
        state = replacement.until(lambda message: message.get('event') == 'status')
        assert state['phase'] == 'speaking' and state['instance'] == instance
        assert len(engine.clients) == 1, 'UI closure must not recreate the engine'
        assert all(message.get('method') != 'turn/interrupt' for message in list(engine.messages.queue))
        command('adapter', message={'event': 'speechFinished', 'request': request})
        adapter.until(lambda message: message.get('phase') == 'idle')

        # A second observer cannot impersonate the media adapter.
        replacement.send({'command': 'trigger', 'instance': instance})
        try:
            while True:
                replacement.line()
        except EOFError:
            pass
        command('trigger'); next_capture = adapter.action('capture')
        assert next_capture['request'] == request + 1
        adapter.close()  # Real media owner loss cancels and retains cleanup.
        def adapter_released():  # A second adapter is refused until the service has dropped the first.
            probe = Peer(port)
            try:
                probe.send({'command': 'hello', 'role': 'observer'})
                return not probe.until(lambda message: message.get('event') == 'status')['adapterConnected']
            finally:
                probe.close()
        wait_for(adapter_released)
        adapter = Peer(port); peers.append(adapter)
        adapter.send({'command': 'hello', 'role': 'adapter'})
        adapter.until(lambda message: message.get('event') == 'status')
        replay = adapter.action('releaseCapture')
        assert replay['sequence'] == unacknowledged['sequence']
        assert replay['path'] == capture['path']
        adapter.action('cancelMedia')
        assert not adapter.action('overlay')['visible']
        assert adapter.action('releaseCapture')['path'] == next_capture['path']
        command('trigger')
        adapter.until(lambda message: message.get('status') == 'In-game platform adapter unavailable')
        # Callbacks from the cancelled question are ignored, never replayed.
        command('adapter', message={'event': 'captured', 'request': next_capture['request']})
        adapter.until(lambda message: message.get('event') == 'ignored')
        assert len(engine.clients) == 1
        command('stop')
        assert process.wait(timeout=5) == 0
        assert engine.errors.empty(), list(engine.errors.queue)
        print('Resident: real sockets, UI detach/reattach, role isolation, media loss and no replay passed')
    finally:
        for peer in peers:
            peer.close()
        if process.poll() is None:
            process.terminate()
            process.wait(timeout=5)
        engine.close()


def backend_test():
    stage = BUILD / 'backend'
    (stage / 'app').mkdir(parents=True, exist_ok=True)
    (stage / 'platform').mkdir(exist_ok=True)
    shutil.copy2(ROOT / 'backend/assistant-service.c', stage / 'app/service.c')
    shutil.copy2(ROOT / 'vendor/ps5-ai-cli/platform/sdk-spawn.h', stage / 'platform/sdk-spawn.h')
    (stage / 'app/config.h').write_text('#define PS5_AI_STATE "/unused-test-runtime"\n')
    (stage / 'app/service-version.h').write_text('#define CODEX_PS5_VERSION "test"\n#define CODEX_BUILD_ID "host-test"\n')
    (stage / 'fake-engine.py').write_text('''import json, os, sys
for line in sys.stdin:
 value = json.loads(line)
 print(json.dumps({'pid':os.getpid(),'echo':value}), flush=True)
 if value.get('finish'): break
''')
    (stage / 'stub.c').write_text(r'''
#include "platform/sdk-spawn.h"
#include <signal.h>
#include <spawn.h>
#include <stdlib.h>
#include <unistd.h>
int ps5_install_runtime_image(void){return 0;}
int ps5_install_runtime_tools(void){return 0;}
pid_t ps5_spawn_sdk(const char *path,char **argv,char **envp,const char *cwd,
 int input,int output,int error,const int *fds,size_t count,int mode){
 (void)path;(void)argv;(void)cwd;(void)mode;
 posix_spawn_file_actions_t actions;posix_spawn_file_actions_init(&actions);
 posix_spawn_file_actions_adddup2(&actions,input,0);
 posix_spawn_file_actions_adddup2(&actions,output,1);
 posix_spawn_file_actions_adddup2(&actions,error,2);
 for(size_t i=0;i<count;++i)if(fds[i]>2)posix_spawn_file_actions_addclose(&actions,fds[i]);
 posix_spawnattr_t attributes;posix_spawnattr_init(&attributes);
 posix_spawnattr_setflags(&attributes,POSIX_SPAWN_SETPGROUP);
 posix_spawnattr_setpgroup(&attributes,0);
 char *args[]={getenv("SERVICE_TEST_PYTHON"),getenv("SERVICE_TEST_ENGINE"),NULL};pid_t child=-1;
 if(posix_spawn(&child,args[0],&actions,&attributes,args,envp))child=-1;
 posix_spawn_file_actions_destroy(&actions);posix_spawnattr_destroy(&attributes);return child;
}
extern int ps5_assistant_run(void);
int main(void){signal(SIGPIPE,SIG_IGN);return ps5_assistant_run();}
''')
    (stage / 'dying-engine.py').write_text('''import os, time
gate, done = os.environ['SERVICE_TEST_GATE'], os.environ['SERVICE_TEST_DONE']
while not os.path.exists(gate): time.sleep(0.01)
import fcntl, struct, termios
unread = struct.unpack('i', fcntl.ioctl(0, termios.FIONREAD, b'\\0\\0\\0\\0'))[0]
os.write(1, b'engine final output\\n')
with open(done, 'w') as marker: marker.write(str(unread))
os._exit(0)  # Never reads stdin: whatever the service forwarded stays unconsumed.
''')

    (stage / 'stubborn-engine.py').write_text('''import json, os, signal, sys, time
for line in sys.stdin:
 value = json.loads(line)
 if value.get('stubborn'):
  signal.signal(signal.SIGTERM, signal.SIG_IGN)  # Only SIGKILL stops this engine.
  print(json.dumps({'pid':os.getpid()}), flush=True)
  while True: time.sleep(0.05)
 print(json.dumps({'pid':os.getpid(),'echo':value}), flush=True)
 if value.get('finish'): break
''')

    (stage / 'graceful-engine.py').write_text('''import json, os, signal, time
gate, term, done = os.environ['SERVICE_TEST_GATE'], os.environ['SERVICE_TEST_TERM'], os.environ['SERVICE_TEST_DONE']
def stop(*_):
 open(term, 'w').close()
 while not os.path.exists(gate): time.sleep(0.01)
 open(done, 'w').close()  # Only written if the engine is allowed to finish exiting on its own.
 os._exit(0)
signal.signal(signal.SIGTERM, stop)
print(json.dumps({'pid':os.getpid()}), flush=True)
while True: time.sleep(0.05)
''')

    def build(name):
        rpc, admin = free_port(), free_port()
        executable = stage / name
        subprocess.run([os.environ.get('CC', 'clang'), '-std=c11', '-D_DEFAULT_SOURCE', '-Wall', '-Wextra', '-Werror',
            f'-DASSISTANT_RPC_PORT={rpc}', f'-DASSISTANT_ADMIN_PORT={admin}',
            str(stage / 'app/service.c'), str(stage / 'stub.c'), '-o', str(executable)], check=True)
        return executable, rpc, admin

    def http(admin, method, path):
        with connect(admin) as sock:
            sock.sendall(f'{method} {path} HTTP/1.1\r\nHost: localhost\r\n\r\n'.encode())
            reply = b''
            while True:
                part = sock.recv(1024)
                if not part:
                    break
                reply += part
            head, body = reply.split(b'\r\n\r\n', 1)
            return head, json.loads(body)

    dying_executable, dying_rpc, dying_admin = build('dying-service')
    executable, rpc, admin = build('service')
    env = {**os.environ, 'SERVICE_TEST_PYTHON': shutil.which('python3'), 'SERVICE_TEST_ENGINE': str(stage / 'fake-engine.py')}
    process = subprocess.Popen([str(executable)], env=env)
    peers = []
    try:
        first, second = Peer(rpc), Peer(rpc); peers.extend((first, second))
        first.send({'owner': 'native'}); first_pid = first.line()['pid']
        second.send({'owner': 'resident'}); second_pid = second.line()['pid']
        assert first_pid != second_pid

        head, body = http(admin, 'GET', '/status')
        assert b'200 OK' in head and body['connections'] == 2 and not body['idle']
        head, _ = http(admin, 'POST', '/stop'); assert b'409 Conflict' in head
        excess = connect(rpc); peers.append(excess)
        assert excess.recv(1) == b'', 'Extra clients must not queue a third engine'
        slow = connect(admin); peers.append(slow); slow.sendall(b'GET /status')
        second.send({'while_control_is_incomplete': True})
        assert second.line()['pid'] == second_pid
        first.close()
        wait_for(lambda: http(admin, 'GET', '/status')[1]['connections'] == 1)
        second.send({'still_alive_after_native_closes': True})
        assert second.line()['pid'] == second_pid
        # Exercise chunking/backpressure and final output after engine EOF.
        second.send({'finish': True, 'large': 'é' * 100000})
        result = second.line()
        assert result['pid'] == second_pid and result['echo']['large'] == 'é' * 100000
        wait_for(lambda: http(admin, 'GET', '/status')[1]['connections'] == 0)
        head, body = http(admin, 'POST', '/stop')
        assert b'200 OK' in head and body['idle']
        assert process.wait(timeout=5) == 0
        print('Backend: two independent engines, nonblocking controls, backpressure, EOF drain and safe stop passed')
    finally:
        for peer in peers:
            peer.close()
        if process.poll() is None:
            process.terminate(); process.wait(timeout=5)

    # The engine exits while the service still holds input it could not deliver.
    gate, done = stage / 'engine-gate', stage / 'engine-done'
    for marker in (gate, done):
        marker.unlink(missing_ok=True)
    env = {**env, 'SERVICE_TEST_ENGINE': str(stage / 'dying-engine.py'),
           'SERVICE_TEST_GATE': str(gate), 'SERVICE_TEST_DONE': str(done)}
    process = subprocess.Popen([str(dying_executable)], env=env)
    peers = []
    try:
        client = Peer(dying_rpc); peers.append(client)
        wait_for(lambda: http(dying_admin, 'GET', '/status')[1]['connections'] == 1)

        def flood():  # More than the queue and the socketpair hold, since the engine never reads.
            try:
                client.sock.sendall(b'x' * 4000000)
            except OSError:
                pass
        threading.Thread(target=flood, daemon=True).start()
        time.sleep(0.5)
        # Stop the service so the engine's output and exit are first seen together.
        process.send_signal(signal.SIGSTOP)
        try:
            def stopped():
                state = subprocess.run(['ps', '-o', 'stat=', '-p', str(process.pid)], capture_output=True, text=True).stdout
                return state.strip().startswith('T')
            wait_for(stopped)  # SIGSTOP is asynchronous: the service must really be frozen.
            gate.touch()
            wait_for(lambda: done.exists() and done.read_text())
            time.sleep(0.2)
        finally:
            process.send_signal(signal.SIGCONT)
        # The engine exited with bytes the service had forwarded but it never consumed.
        assert int(done.read_text()) > 0, 'Engine must exit with unread input'
        received = b''
        try:
            while True:
                part = client.sock.recv(65536)
                if not part:
                    break
                received += part
        except ConnectionResetError:
            pass  # The service closed with client bytes it never read.
        assert received == b'engine final output\n', received
        wait_for(lambda: http(dying_admin, 'GET', '/status')[1]['connections'] == 0)
        head, body = http(dying_admin, 'POST', '/stop')
        assert b'200 OK' in head and body['idle']
        assert process.wait(timeout=5) == 0
        print('Backend: engine exit with unconsumed input still delivers its final output')
    finally:
        for peer in peers:
            peer.close()
        if process.poll() is None:
            process.send_signal(signal.SIGCONT)
            process.terminate(); process.wait(timeout=5)

    # Tearing down an engine that ignores SIGTERM must not stall the other session.
    stubborn_executable, stubborn_rpc, stubborn_admin = build('stubborn-service')
    env = {**env, 'SERVICE_TEST_ENGINE': str(stage / 'stubborn-engine.py')}
    process = subprocess.Popen([str(stubborn_executable)], env=env)
    peers = []
    try:
        stubborn, other = Peer(stubborn_rpc), Peer(stubborn_rpc); peers.extend((stubborn, other))
        stubborn.send({'stubborn': True}); stubborn_pid = stubborn.line()['pid']
        other.send({'hello': 1}); other_pid = other.line()['pid']
        def alive():
            return bool(subprocess.run(['ps', '-o', 'stat=', '-p', str(stubborn_pid)], capture_output=True, text=True).stdout.strip())
        assert alive()
        stubborn.close()
        # Once the service reports one connection it has started tearing the stubborn engine down.
        wait_for(lambda: http(stubborn_admin, 'GET', '/status')[1]['connections'] == 1)
        other.send({'during_teardown': True})
        assert other.line()['pid'] == other_pid
        # A blocking teardown would only have answered after SIGKILL and reaping.
        assert alive(), 'The other session was served only after the stubborn engine was gone'
        wait_for(lambda: not alive())  # SIGKILL after the grace period, then the zombie is collected.
        other.send({'finish': True}); other.line()
        wait_for(lambda: http(stubborn_admin, 'GET', '/status')[1]['connections'] == 0)
        head, body = http(stubborn_admin, 'POST', '/stop')
        assert b'200 OK' in head and body['idle']
        assert process.wait(timeout=5) == 0
        print('Backend: a SIGTERM-ignoring engine is reaped without stalling the other session')
    finally:
        for peer in peers:
            peer.close()
        if process.poll() is None:
            process.terminate(); process.wait(timeout=5)

    # /stop right after a disconnect must let the engine finish exiting after SIGTERM, not SIGKILL it.
    graceful_executable, graceful_rpc, graceful_admin = build('graceful-service')
    term = stage / 'engine-term'
    for marker in (gate, term, done):
        marker.unlink(missing_ok=True)
    env = {**env, 'SERVICE_TEST_ENGINE': str(stage / 'graceful-engine.py'), 'SERVICE_TEST_TERM': str(term)}
    process = subprocess.Popen([str(graceful_executable)], env=env)
    peers = []
    try:
        client = Peer(graceful_rpc); peers.append(client)
        client.line()  # The engine has installed its SIGTERM handler.
        client.close()
        wait_for(term.exists)  # The service sent SIGTERM and the engine is still exiting.
        head, body = http(graceful_admin, 'POST', '/stop')
        assert b'200 OK' in head and body['idle']
        gate.touch()
        assert process.wait(timeout=5) == 0
        assert done.exists(), 'Shutdown killed an engine that was still exiting cleanly'
        print('Backend: shutdown gives a terminating engine its SIGTERM grace period')
    finally:
        for peer in peers:
            peer.close()
        gate.touch()
        if process.poll() is None:
            process.terminate(); process.wait(timeout=5)


if __name__ == '__main__':
    resident_test()
    backend_test()
