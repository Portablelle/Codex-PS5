#!/usr/bin/env python3
"""Host observer CLI; closing it never closes the resident Codex connection."""
import argparse
import json
import socket

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('command', choices=('status', 'watch', 'stop'))
parser.add_argument('--port', type=int, default=49324)
args = parser.parse_args()
if not 1 <= args.port <= 65535:
    parser.error('Port must be between 1 and 65535')

try:
    with socket.create_connection(('127.0.0.1', args.port), timeout=3) as sock:
        with sock.makefile('rb') as stream:
            sock.sendall(b'{"command":"hello","role":"observer"}\n')
            line = stream.readline(1048577)
            if not line.endswith(b'\n') or len(line) > 1048576:
                raise ValueError('Invalid resident handshake')
            status = json.loads(line)
            if status.get('event') != 'status':
                raise ValueError('Expected resident status')
            if args.command == 'stop':
                sock.sendall(json.dumps({'command': 'stop', 'instance': status['instance']}).encode() + b'\n')
            else:
                print(json.dumps(status, ensure_ascii=False), flush=True)
                if args.command == 'status':
                    raise SystemExit(0)
                sock.settimeout(None)
            while True:
                line = stream.readline(1048577)
                if not line:
                    break
                if not line.endswith(b'\n') or len(line) > 1048576:
                    raise ValueError('Invalid resident frame')
                message = json.loads(line)
                if message.get('event') == 'error':
                    raise ValueError(message.get('message', 'Resident command failed'))
                if args.command == 'watch':
                    print(json.dumps(message, ensure_ascii=False), flush=True)
except KeyboardInterrupt:
    pass
except (OSError, ValueError, KeyError) as error:
    parser.exit(1, f'{error}\n')
