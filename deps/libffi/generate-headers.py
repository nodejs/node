#!/usr/bin/env python3

import os
import sys
import shutil
import re
import platform

def generate_headers(output_dir):
    """Generate minimal libffi headers."""
    os.makedirs(output_dir, exist_ok=True)

    base_dir = os.path.dirname(__file__)

    # Copy common headers from msvc_build/aarch64
    msvc_dir = os.path.join(base_dir, 'msvc_build', 'aarch64', 'aarch64_include')
    common_headers = ['ffi.h', 'fficonfig.h']

    for header in common_headers:
        src = os.path.join(msvc_dir, header)
        dst = os.path.join(output_dir, header)

        if os.path.exists(src):
            shutil.copy2(src, dst)
            print(f'Copied {header}')
        else:
            print(f'Warning: {src} not found', file=sys.stderr)

    # On macOS, we need to enable FFI_EXEC_TRAMPOLINE_TABLE for W^X compliance
    if platform.system() == 'Darwin':
        fficonfig_h_path = os.path.join(output_dir, 'fficonfig.h')
        if os.path.exists(fficonfig_h_path):
            with open(fficonfig_h_path, 'r') as f:
                content = f.read()

            # Replace #undef FFI_EXEC_TRAMPOLINE_TABLE with #define
            content = content.replace(
                '/* #undef FFI_EXEC_TRAMPOLINE_TABLE */',
                '#define FFI_EXEC_TRAMPOLINE_TABLE 1'
            )

            with open(fficonfig_h_path, 'w') as f:
                f.write(content)
            print('Patched fficonfig.h to enable FFI_EXEC_TRAMPOLINE_TABLE for macOS')

        # Also fix ffi.h to use FFI_EXEC_TRAMPOLINE_TABLE instead of hardcoded #if 0
        ffi_h_path = os.path.join(output_dir, 'ffi.h')
        if os.path.exists(ffi_h_path):
            with open(ffi_h_path, 'r') as f:
                content = f.read()

            # Replace the #if 0 with #if FFI_EXEC_TRAMPOLINE_TABLE for trampoline fields
            content = content.replace(
                '''typedef struct {
#if 0
  void *trampoline_table;
  void *trampoline_table_entry;
#else
  char tramp[FFI_TRAMPOLINE_SIZE];
#endif''',
                '''typedef struct {
#if FFI_EXEC_TRAMPOLINE_TABLE
  void *trampoline_table;
  void *trampoline_table_entry;
#else
  char tramp[FFI_TRAMPOLINE_SIZE];
#endif'''
            )

            with open(ffi_h_path, 'w') as f:
                f.write(content)
            print('Patched ffi.h to conditionally enable trampoline_table fields for macOS')

    # Now patch ffi.h to add missing defines if needed
    ffi_h_path = os.path.join(output_dir, 'ffi.h')
    if os.path.exists(ffi_h_path):
        with open(ffi_h_path, 'r') as f:
            content = f.read()

        # Fix ffi_status enum to include FFI_BAD_ARGTYPE (needed for newer libffi sources)
        old_enum = '''typedef enum {
  FFI_OK = 0,
  FFI_BAD_TYPEDEF,
  FFI_BAD_ABI
} ffi_status;'''

        new_enum = '''typedef enum {
  FFI_OK = 0,
  FFI_BAD_TYPEDEF,
  FFI_BAD_ABI,
  FFI_BAD_ARGTYPE
} ffi_status;'''

        if old_enum in content:
            content = content.replace(old_enum, new_enum)
            print('Patched ffi.h with FFI_BAD_ARGTYPE enum value')

        # Add version macros right before the final #endif if not present
        if 'FFI_VERSION_STRING' not in content:
            version_patch = '\n#define FFI_VERSION_STRING "3.5.2"\n#define FFI_VERSION_NUMBER 350002UL\n'
            # Insert before the final #endif
            if content.endswith('\n#endif\n'):
                content = content[:-8] + version_patch + '#endif\n'
            elif content.endswith('#endif'):
                content = content[:-6] + version_patch + '#endif'
            else:
                content = content.rstrip() + version_patch
            print('Patched ffi.h with version defines')

        with open(ffi_h_path, 'w') as f:
            f.write(content)

    # Copy platform-specific ffitarget.h from src/aarch64
    ffitarget_src = os.path.join(base_dir, 'src', 'aarch64', 'ffitarget.h')
    ffitarget_dst = os.path.join(output_dir, 'ffitarget.h')

    if os.path.exists(ffitarget_src):
        shutil.copy2(ffitarget_src, ffitarget_dst)
        print(f'Copied ffitarget.h from aarch64')
    else:
        print(f'Warning: {ffitarget_src} not found', file=sys.stderr)

if __name__ == '__main__':
    if len(sys.argv) > 1:
        generate_headers(sys.argv[1])
    else:
        print('Usage: generate-headers.py <output_dir>', file=sys.stderr)
        sys.exit(1)
