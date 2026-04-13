#!/usr/bin/env python3

import argparse
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import sys


def split_command(value):
    if not value:
        return None
    value = value.strip()
    if not value:
        return None
    return shlex.split(value, posix=(os.name != 'nt'))


def find_compiler():
    for var in ('CC', 'CXX'):
        command = split_command(os.environ.get(var))
        if command and shutil.which(command[0]):
            return command

    for name in ('cl.exe', 'cl', 'clang-cl.exe', 'clang-cl', 'cc', 'clang', 'gcc'):
        path = shutil.which(name)
        if path:
            return [path]

    raise RuntimeError('Unable to locate a compiler for preprocessing assembly')


def normalize_path(value):
    return str(value).strip().strip('"')


def unique_paths(paths):
    seen = set()
    result = []
    for path in paths:
        if path in seen:
            continue
        seen.add(path)
        result.append(path)
    return result


def preprocess(args):
    compiler = find_compiler()
    input_path = normalize_path(args.input)
    output = Path(normalize_path(args.output))
    include_dirs = [normalize_path(include_dir) for include_dir in args.include_dir]
    include_dirs.append(str(output.parent))
    include_dirs = unique_paths(include_dirs)
    output.parent.mkdir(parents=True, exist_ok=True)

    if os.name == 'nt' and Path(compiler[0]).name.lower() in ('cl.exe', 'cl', 'clang-cl.exe', 'clang-cl'):
        command = compiler + ['/nologo', '/EP', '/TC']
        command += [f'/I{include_dir}' for include_dir in include_dirs]
        command += [f'/D{define}' for define in args.define]
        command += [input_path]
    else:
        command = compiler + ['-E', '-P', '-x', 'c']
        command += [f'-I{include_dir}' for include_dir in include_dirs]
        command += [f'-D{define}' for define in args.define]
        command += [input_path]

    result = subprocess.run(command, capture_output=True, text=True)
    if result.returncode != 0:
        sys.stderr.write(result.stderr)
        raise RuntimeError(f'Preprocessing failed: {" ".join(command)}')

    # Strip preprocessor line directives that some assemblers can't handle
    # (e.g., armasm64.exe doesn't accept #line directives)
    cleaned = re.sub(r'^#\s*\d+\s+"[^"]*".*$', '', result.stdout, flags=re.MULTILINE)
    cleaned = re.sub(r'^#\s*line\s+\d+.*$', '', cleaned, flags=re.MULTILINE)

    output.write_text(cleaned, encoding='utf-8')


def main(argv=None):
    parser = argparse.ArgumentParser(description='Preprocess libffi assembly source')
    parser.add_argument('--input', required=True)
    parser.add_argument('--output', required=True)
    parser.add_argument('--include-dir', action='append', default=[])
    parser.add_argument('--define', action='append', default=[])
    args = parser.parse_args(argv)

    preprocess(args)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
