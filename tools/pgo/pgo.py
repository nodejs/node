#!/usr/bin/env python3
"""Train a Clang PGO build and merge its profiles into node.profdata."""

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


def find_llvm_profdata():
  override = os.environ.get('LLVM_PROFDATA')
  if override:
    tool = shutil.which(override)
  elif sys.platform == 'darwin':
    tool = subprocess.check_output(
      ['xcrun', '--find', 'llvm-profdata'], text=True).strip()
  else:
    candidates = []
    if sys.platform == 'win32':
      vc_install = os.environ.get('VCINSTALLDIR')
      if vc_install:
        candidates.append(Path(vc_install) / 'Tools/Llvm/x64/bin/llvm-profdata.exe')
      program_files = os.environ.get('ProgramFiles')
      if program_files:
        candidates.extend(
          Path(program_files) / 'Microsoft Visual Studio' / version / edition /
          'VC/Tools/Llvm/x64/bin/llvm-profdata.exe'
          for version in ('2026', '2022')
          for edition in ('Enterprise', 'Community', 'Professional', 'BuildTools'))
    tool = next((str(path) for path in candidates if path.is_file()), None)
    if tool is None:
      tool = shutil.which('llvm-profdata')
  if not tool:
    raise ValueError('llvm-profdata not found. Set LLVM_PROFDATA to the tool '
                     'from your Clang toolchain.')
  return tool


def main():
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('--duration', type=int, default=15,
                      help='seconds per workload (default: 15)')
  args = parser.parse_args()
  if args.duration <= 0:
    parser.error('--duration must be a positive integer')

  repo_root = Path(__file__).resolve().parents[2]
  if sys.platform == 'win32':
    node = repo_root / 'Release/node.exe'
    build = 'vcbuild.bat pgo-generate'
    rebuild = 'vcbuild.bat pgo-use'
  else:
    node = repo_root / 'out/Release/node'
    build = './configure --ninja --enable-pgo-generate && make'
    rebuild = './configure --ninja --enable-pgo-use && make'
  if not node.is_file():
    raise ValueError(f'Instrumented binary not found: {node}\nBuild with: {build}')
  llvm_profdata = find_llvm_profdata()

  profile_dir = Path(tempfile.mkdtemp(prefix='pgo-profiles.', dir=repo_root))
  env = os.environ.copy()
  env['LLVM_PROFILE_FILE'] = str(profile_dir / 'node-%m-%p.profraw')

  print(f'Training {node} for {args.duration}s per workload', flush=True)
  print(f'Profile directory: {profile_dir}', flush=True)
  try:
    subprocess.run([str(node), str(repo_root / 'tools/pgo/pgo-run-all.js'),
                    f'--duration={args.duration}', '--verbose'],
                   cwd=repo_root, env=env, check=True)
    profiles = list(profile_dir.glob('*.profraw'))
    if not profiles:
      raise ValueError(f'No .profraw files found in {profile_dir}. '
                       'Build Node with PGO instrumentation using Clang.')

    print(f'Merging {len(profiles)} profiles with {llvm_profdata}', flush=True)
    merged = profile_dir / 'node.profdata'
    subprocess.run([llvm_profdata, 'merge', '-o', str(merged),
                    *map(str, profiles)], check=True)
    merged.replace(repo_root / 'node.profdata')
  except (OSError, ValueError, subprocess.CalledProcessError):
    print(f'PGO failed. Collected profiles are in: {profile_dir}', file=sys.stderr)
    raise
  shutil.rmtree(profile_dir)
  print(f'Profile data: {repo_root / "node.profdata"}')
  print(f'Next step: {rebuild}')


if __name__ == '__main__':
  main()
