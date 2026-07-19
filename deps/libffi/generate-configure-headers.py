#!/usr/bin/env python3

import os
import sys
import subprocess
import argparse
from pathlib import Path

def get_host_os(os_name):
    """Map GYP OS names to configure host OS."""
    mapping = {
        'linux': 'linux-gnu',
        'mac': 'darwin',
        'win': 'mingw',
        'freebsd': 'freebsd',
    }
    return mapping.get(os_name, os_name)

def get_host_arch(os_name, arch):
    """Map GYP architecture to configure host architecture."""
    arch_mapping = {
        'x64': 'x86_64',
        'x86': 'i686',
        'arm': 'arm',
        'arm64': 'aarch64',
    }
    arch_name = arch_mapping.get(arch, arch)
    os_name_mapped = get_host_os(os_name)
    return f"{arch_name}-pc-{os_name_mapped}"

def run_command(cmd):
    """Run a command and return success status."""
    try:
        subprocess.run(cmd, check=True)
        return True
    except subprocess.CalledProcessError as e:
        print(f"Error running command: {' '.join(cmd)}", file=sys.stderr)
        print(f"Error: {e}", file=sys.stderr)
        return False

def generate_headers(output_dir, target_arch, os_name, libffi_dir):
    """Generate libffi headers using configure script."""
    os.makedirs(output_dir, exist_ok=True)

    # Change to libffi directory
    original_dir = os.getcwd()
    try:
        os.chdir(libffi_dir)

        # Create a temporary build directory
        build_dir = os.path.join(output_dir, 'build')
        os.makedirs(build_dir, exist_ok=True)

        host_triplet = get_host_arch(os_name, target_arch)

        # Run configure script
        configure_path = os.path.join(libffi_dir, 'configure')
        configure_cmd = [
            configure_path,
            f'--host={host_triplet}',
            f'--prefix={build_dir}',
            '--disable-shared',
            '--enable-static',
        ]

        print(f"Running configure with: {' '.join(configure_cmd)}")

        if not run_command(configure_cmd):
            return False

        # Copy generated headers to output directory
        header_files = ['fficonfig.h', 'ffitarget.h', 'include/ffi.h']

        for header in header_files:
            src_path = os.path.join(libffi_dir, header)
            dst_path = os.path.join(output_dir, os.path.basename(header))

            if os.path.exists(src_path):
                with open(src_path, 'r') as src:
                    with open(dst_path, 'w') as dst:
                        dst.write(src.read())
                print(f"Generated: {dst_path}")
            else:
                print(f"Warning: {src_path} not found", file=sys.stderr)

        return True

    finally:
        os.chdir(original_dir)

def main():
    parser = argparse.ArgumentParser(description='Generate libffi headers')
    parser.add_argument('--output-dir', required=True, help='Output directory for generated headers')
    parser.add_argument('--target-arch', required=True, help='Target architecture (x86, x64, arm, arm64)')
    parser.add_argument('--os', required=True, help='Target OS (linux, mac, win, freebsd)')

    args = parser.parse_args()

    # Get the libffi directory (parent of this script)
    libffi_dir = os.path.dirname(os.path.abspath(__file__))

    if generate_headers(args.output_dir, args.target_arch, args.os, libffi_dir):
        print("Headers generated successfully")
        sys.exit(0)
    else:
        print("Failed to generate headers", file=sys.stderr)
        sys.exit(1)

if __name__ == '__main__':
    main()
