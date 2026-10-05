#!/usr/bin/env python3
"""Produce the native app-server variant without editing the upstream snapshot."""
from pathlib import Path
import shutil
root=Path(__file__).resolve().parents[1]
destination=root/'.deps/backend'
shutil.copytree(root/'vendor/ps5-ai-cli',destination,dirs_exist_ok=True,
                ignore=shutil.ignore_patterns('.git','AGENTS.md','build','target','__pycache__'))
shutil.copy2(root/'backend/assistant-service.c',destination/'app/assistant-service.c')
for name in ('df','ps','sysctl','tools'):
 shutil.copy2(root/f'backend/ps5-{name}.c',destination/f'platform/ps5-{name}.c')
shutil.copy2(root/'backend/tool-compat.c',destination/'platform/tool-compat.c')
shutil.copy2(root/'backend/runtime-tools.txt',destination/'platform/runtime-tools.txt')
p=destination/'app/config.h'
p.write_text(p.read_text().replace('"/data/ps5-ai-cli"','"/data/codex-ps5"'))
p=destination/'app/entry.c'
s=p.read_text().replace('extern int ps5_install_runtime_image(void);',
 'extern int ps5_install_runtime_image(void);\nextern int ps5_assistant_run(void);')
needle='    signal(SIGPIPE, SIG_IGN);\n    char code[7];'
assert s.count(needle)==1
p.write_text(s.replace(needle,'    signal(SIGPIPE, SIG_IGN);\n    return ps5_assistant_run();\n    char code[7];'))
for name in ('build-terminal.sh','ps5-rust-linker.sh'):
 p=destination/'tools'/name
 s=p.read_text();needle='runtime-tools trust-store; do'
 assert s.count(needle)==1
 p.write_text(s.replace(needle,'runtime-tools trust-store assistant-service; do'))
print(destination)

p=destination/'app/runtime-image.c'
s=p.read_text().replace('ps5-ai-cli','assistant-service')
s=s.replace('strncmp(name,"assistant-service-",11)','strncmp(name,"assistant-service-",18)')
s=s.replace('length<16','length<23').replace('strncmp(filename,"assistant-service-",11)','strncmp(filename,"assistant-service-",18)').replace('size_t i=11','size_t i=18')
p.write_text(s)

# The executable-path shim validates a literal path, independently of config.h.
p=destination/'platform/executable-path.c'
s=p.read_text()
assert s.count('/data/ps5-ai-cli')==2
p.write_text(s.replace('/data/ps5-ai-cli','/data/codex-ps5'))

# Relapse setups can install the payload over FTP without Payload Manager.
p=destination/'app/runtime-image.c'
s=p.read_text()
needle='DIR *dir=opendir(PS5_AI_PAYLOAD_ROOT);'
assert s.count(needle)==1
p.write_text(s.replace(needle,
 'DIR *dir=opendir("/data/codex-ps5/payloads");\n'
 '    if (!dir) dir=opendir(PS5_AI_PAYLOAD_ROOT);'))

p=destination/'tools/check-codex.sh'
s=p.read_text()
assert s.count('--max-size-mib 512')==1
p.write_text(s.replace('--max-size-mib 512','--max-size-mib 256'))

# Native Prospero commands complement the portable sbase utilities.
native_names=('df','ps','sysctl','tools')
p=destination/'tools/build-shell.sh'
s=p.read_text().replace('platform/tool-stdio.c', 'platform/tool-stdio.c platform/tool-compat.c').replace('libutf.a libutil.a\n', 'libutf.a libutil.a\n# Namespace the portable allocator; keep ereallocarray/enreallocarray intact.\n"$cc" -O2 -D__BSD_VISIBLE=1 -c libutil/reallocarray.c -o libutil/reallocarray.o\nllvm-objcopy --redefine-sym reallocarray=sbase_reallocarray libutil/reallocarray.o\n/opt/ps5-payload-sdk/bin/prospero-ar r libutil.a libutil/reallocarray.o\n')
p.write_text(s+''.join(f'\n"$cc" -O2 -Wall -Wextra -Werror platform/ps5-{name}.c -o build/shell/{name}.elf\n' for name in native_names))
p=destination/'tools/embed-runtime.py'
s=p.read_text().replace("box = (root / 'build/shell/sbase-box.elf').read_bytes()",
 "box = (root / 'build/shell/sbase-box.elf').read_bytes()\n"+
 '\n'.join(f"{name} = (root / 'build/shell/{name}.elf').read_bytes()" for name in native_names))
s=s.replace('shell + box +', 'shell + box + '+' + '.join(native_names)+' +')
s=s.replace("('box', box)]", "('box', box), "+', '.join(f"('{name}', {name})" for name in native_names)+"]")
s=s.replace("lines.append('};')\n(root / 'build/runtime-tools.h')",
 '\n'.join(f"lines.append('{{\"{'ps5-tools' if name=='tools' else name}\", runtime_{name}, sizeof(runtime_{name})}},')" for name in native_names)+"\nlines.append('};')\n(root / 'build/runtime-tools.h')")
p.write_text(s)
