#!/usr/bin/env python3
"""Remove C++ stream dependencies from the pinned native Whisper build."""
from pathlib import Path
import shutil,sys
source,destination=map(Path,sys.argv[1:])
shutil.copytree(source,destination,dirs_exist_ok=True,
 ignore=shutil.ignore_patterns('.git','models','build','__pycache__','whisper.cpp'))
p=destination/'src/whisper.cpp';s=(source/'src/whisper.cpp').read_text()
s=s.replace('#include <fstream>','#include <cstdio>')
def replace_block(text,needle,replacement):
 start=text.index(needle);opening=text.index('{',start);level=1;end=opening+1
 while level:
  if text[end]=='{':level+=1
  if text[end]=='}':level-=1
  end+=1
 return text[:start]+replacement+text[end:]
# The optional feature dump is deliberately disabled: no audio-derived files.
s=replace_block(s,'    if (debug) {\n        std::ofstream','    (void)debug; // Native PS5: keep microphone-derived data in memory.')
loader='''
    FILE * file = fopen(path_model, "rb");
    if (!file) return nullptr;
    whisper_model_loader loader = {};
    loader.context = file;
    loader.read = [](void * ctx, void * out, size_t size) {return fread(out, 1, size, (FILE *)ctx);};
    loader.eof = [](void * ctx) {return feof((FILE *)ctx) != 0;};
    loader.close = [](void * ctx) {fclose((FILE *)ctx);};
'''
s=replace_block(s,'struct whisper_context * whisper_init_from_file_with_params_no_state(',
 '''struct whisper_context * whisper_init_from_file_with_params_no_state(const char * path_model, struct whisper_context_params params) {'''+loader+'''
    auto ctx = whisper_init_with_params_no_state(&loader, params);
    if (ctx) ctx->path_model = path_model;
    return ctx;
}''')
s=replace_block(s,'struct whisper_vad_context * whisper_vad_init_from_file_with_params(',
 '''struct whisper_vad_context * whisper_vad_init_from_file_with_params(const char * path_model, struct whisper_vad_context_params params) {'''+loader+'''
    auto ctx = whisper_vad_init_with_params(&loader, params);
    if (ctx) ctx->path_model = path_model;
    return ctx;
}''')
assert 'std::ifstream' not in s and 'std::ofstream' not in s
if not p.exists() or p.read_text()!=s:p.write_text(s)
