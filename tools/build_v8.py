#!/usr/bin/env python3

import os
import sys
import subprocess
import platform
import shlex
import shutil
from pathlib import Path
from argparse import ArgumentParser

from v8.fetch_deps import FetchDeps
from v8.node_common import UninitGit
from utils import IsWindows

v8_arch_map = {
    "amd64": "x64",
    "x86_64": "x64",
    "i386": "x86",
    "i686": "x86",
    "arm64": "arm64",
    "aarch64": "arm64",
}

class V8Builder:
    """Handles the building of V8 for different architectures."""

    def __init__(self, build_arch_type, jobs=None, v8_build_options=None):
        self.build_arch_type = build_arch_type
        self.jobs = jobs
        self.v8_build_options = v8_build_options
        self.arch = platform.machine()
        self.v8_dir = Path('deps/v8').resolve()

    def fetch_dependencies(self):
        """Initialize repository and fetch dependencies."""
        UninitGit(str(self.v8_dir))
        self.depot_tools = FetchDeps(str(self.v8_dir))

    def get_ninja_jobs_arg(self):
        """Return jobs argument for ninja if specified."""
        return ['-j', str(self.jobs)] if self.jobs else []

    def create_symlink(self, env_var, default_bin, build_tools):
        """Create compiler symlink if a custom compiler is specified."""
        custom_compiler = os.environ.get(env_var)

        if not custom_compiler or custom_compiler == default_bin:
            return

        bin_path = shutil.which(custom_compiler)
        if not bin_path or 'ccache' in bin_path:
            return

        link_path = Path(build_tools) / default_bin
        try:
            if link_path.exists() or link_path.is_symlink():
                link_path.unlink()
            link_path.symlink_to(bin_path)
        except OSError as e:
            print(f"Failed to create symlink: {e}")

    def build_special_arch(self):
        """Build for s390x or ppc64le architectures."""
        target_arch = 'ppc64' if self.arch == 'ppc64le' else self.arch
        build_tools = '/home/iojs/build-tools'

        env = os.environ.copy()
        env['BUILD_TOOLS'] = build_tools
        env['LD_LIBRARY_PATH'] = f"{build_tools}:{env.get('LD_LIBRARY_PATH', '')}"
        env['PATH'] = f"{build_tools}:{env['PATH']}"
        env['PKG_CONFIG_PATH'] = f"{build_tools}/pkg-config"

        self.create_symlink('CC', 'gcc', build_tools)
        self.create_symlink('CXX', 'g++', build_tools)

        cc_wrapper = 'cc_wrapper="ccache"' if 'CXX' in env and 'ccache' in env['CXX'] else ""

        gn_args = (
            f"is_component_build=false is_debug=false use_goma=false "
            f"goma_dir=\"None\" use_custom_libcxx=false v8_target_cpu=\"{target_arch}\" "
            f"target_cpu=\"{target_arch}\" v8_enable_backtrace=true {cc_wrapper}"
        )

        out_dir = f"out.gn/{self.build_arch_type}"
        subprocess.run(['gn', 'gen', '-v', out_dir, f"--args={gn_args}"], env=env, check=True)

        ninja_cmd = ['ninja', '-v', '-C', out_dir]
        ninja_cmd.extend(self.get_ninja_jobs_arg())
        ninja_cmd.extend(['d8', 'cctest', 'inspector-test'])
        subprocess.run(ninja_cmd, env=env, check=True)

    def build_standard_arch(self):
        """Build for standard architectures using depot_tools."""
        env = os.environ.copy()
        env['PATH'] = f"{self.depot_tools}:{env.get('PATH', '')}"

        v8gen_cmd = [sys.executable, 'tools/dev/v8gen.py', '-vv', self.build_arch_type]
        if self.v8_build_options:
            v8gen_cmd.extend(shlex.split(self.v8_build_options))
        if IsWindows():
            env["DEPOT_TOOLS_WIN_TOOLCHAIN"] = "0"

        subprocess.run(v8gen_cmd, env=env, check=True)

        out_dir = f"out.gn/{self.build_arch_type}"
        ninja_cmd = ['ninja', '-C', out_dir]
        ninja_cmd.extend(self.get_ninja_jobs_arg())
        ninja_cmd.extend(['d8', 'cctest', 'inspector-test'])
        subprocess.run(ninja_cmd, env=env, check=True)

    def build(self):
        """Main build function that orchestrates the build process."""
        os.chdir(self.v8_dir)
        self.fetch_dependencies()

        if self.arch in ['s390x', 'ppc64le']:
            self.build_special_arch()
        else:
            self.build_standard_arch()

def main():
    """Parse arguments and initiate the V8 build."""
    parser = ArgumentParser(description='Build V8 with specified configuration')
    parser.add_argument('build_arch_type', nargs='?', default=f"{v8_arch_map.get(platform.machine(), 'x64')}.release",
                        help='Build architecture type (e.g., x64.release)')
    parser.add_argument('-j', '--jobs', type=int,
                        help='Number of jobs to run simultaneously')
    parser.add_argument('--v8-build-options', type=str, default=None,
                        help='Additional V8 build options as a quoted string')

    args = parser.parse_args()

    builder = V8Builder(args.build_arch_type, args.jobs, args.v8_build_options)
    builder.build()

if __name__ == '__main__':
    main()
