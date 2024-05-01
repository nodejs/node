#!/usr/bin/env python3

import sys
import shutil
import fnmatch
import os
import importlib
import subprocess
import glob
import re
import pathlib
import json
import tarfile
import tempfile
import gzip

# try to get 'requests', if not found, set HAS_REQUESTS to False
try:
    import requests
    HAS_REQUESTS = True
except ImportError:
    HAS_REQUESTS = False

def import_module(name, attr):
    try:
        return getattr(importlib.import_module(name), attr)
    except ImportError:
        print(f"Error: {name} module not found")
        sys.exit(1)

def anti_glob(pattern, files):
    return [file for file in files if not fnmatch.fnmatch(file, pattern)]

def max_depth(files, depth):
    return [file for file in files if file.count(os.path.sep) <= depth]

class CLI:
    @staticmethod
    def args():
        # return all the args AFTER build.py
        return sys.argv[sys.argv.index('build.py') + 1:]

    @staticmethod
    def has(key):
        for arg in CLI.args():
            if arg.startswith(key):
                return True
    @staticmethod
    def get(key):
        args = CLI.args()
        for arg in args:
            if arg.startswith(key):
                print(arg)
                if arg == key:
                    return args[args.index(arg) + 1]
                if arg[len(key)] == '=':
                    return arg[len(key) + 1:]
                return arg[len(key):]
        return None

    @staticmethod
    def unescape(value):
        if value[0] in ['"', "'"]:
            value = value[1:]

        if value[-1] in ['"', "'"]:
            value = value[:-1]

        return value

    @staticmethod
    def get_cli_env():
        env = {}
        for arg in CLI.args():
            if not arg.startswith('-') and '=' in arg:
                key, value = arg.split('=')
                env[CLI.unescape(key)] = CLI.unescape(value)
        return env

    @staticmethod
    def get_target_arg():
        for arg in CLI.args():
            if not arg.startswith('-') and '=' not in arg:
                return arg
        return None

class Logger:
    def __init__(self, loglevel):
        if type(loglevel) is str:
            match loglevel.lower():
                case 'error':
                    self.loglevel = 1
                case 'warn':
                    self.loglevel = 2
                case 'info':
                    self.loglevel = 3
                case 'debug':
                    self.loglevel = 4
                case _:
                    self.loglevel = 3
        elif type(loglevel) is int:
            self.loglevel = loglevel
        else:
            self.loglevel = 3

    def error(self, message):
        if self.loglevel >= 1:
            Logger.print("ERROR", "31", message)

    def warn(self, message):
        if self.loglevel >= 2:
            Logger.print("WARN", "33", message)

    def info(self, message):
        if self.loglevel >= 3:
            Logger.print("INFO", "32", message)

    def debug(self, message):
        if self.loglevel >= 4:
            Logger.print("DEBUG", "34", message)

    @staticmethod
    def print(level, color, message):
        print(f"\033[1m\033[{color}m{level}\033[0m {message}")

class Environment:
    def __init__(self, env):
        self.env = env

    def string(self, key, default):
        value = self.env.get(key)
        if value is None:
            return default
        return value

    def array(self, key, default):
        value = self.env.get(key)
        if value is None:
            return default
        return value.replace(",", " ").split(" ")

    def int(self, key, default):
        value = self.env.get(key)
        if value is None:
            return default
        return int(value)

def convert_to_junit(path):
    conv_func = import_module('./tools/v8-json-to-junit.py', 'v8-json-to-junit')
    with open(path, 'r') as f:
        out = conv_func.Main(f.read())
        out_file = open(f'{path.replace(".json", ".xml")}', 'w')
        out_file.write(out)
        out_file.close()
        f.close()

class BuildOptions:
    def persist(self, key, func):
        if key in self.persistent_values:
            return self.persistent_values[key]
        value = func()
        self.persistent_values[key] = value
        return value

    def initialize(self):
        if not os.path.exists('config.mk'):
            Logger.print('ERROR', '31', 'Error: config.mk not found, run configure first')
            sys.exit(1)

        with open('config.mk', 'r') as f:
            for line in f:
                match = re.search(r'(\w+)\s*=\s*(.*)', line)
                if match:
                    self.environment.env[match.group(1)] = match.group(2)
            f.close()

        extra_env = CLI.get_cli_env()
        self.environment.env.update(extra_env)

    def __init__(self, env=None, options={}, persistent_values={}, do_init=False):
        self.environment = Environment(env if env is not None else os.environ)
        self.persistent_values = persistent_values
        if do_init: self.initialize()
        uname = os.uname()
        self.options = {
            'build_type': self.environment.string('BUILDTYPE', 'Release'),
            'build_with': self.environment.string('BUILD_WITH', 'make'),
            'log_level': self.environment.int('LOG_LEVEL', 3),
            'ninja': self.environment.string('NINJA', 'ninja'),
            'dest_dir': self.environment.string('DESTDIR', ''),
            'prefix': self.environment.string('PREFIX', '/usr/local'),
            'distribution_type': self.environment.string('DISTTYPE', 'release'),
            'coverage_tests': self.environment.string('COVTESTS', 'test-cov'),
            'test_ci_args': self.environment.array('TEST_CI_ARGS', []),
            'gcov_executable': self.environment.string('GCOV', 'gcov'),
            'gtest_filter': self.environment.string('GTEST_FILTER', '*'),
            'v8_build_options': self.environment.array('V8_BUILD_OPTIONS', []),
            'ci_skip_tests': self.environment.array('CI_SKIP_TESTS', []),
            'js_suites': self.environment.array('JS_SUITES', ['default']),
            'native_suites': self.environment.array('NATIVE_SUITES', ['addons', 'js-native-api', 'node-api']),
            'coverage_skip_tests': self.environment.array('COV_SKIP_TESTS', ['core_line_numbers.js', 'testFinalizer.js', 'test_function/test.js']),
            'flaky_tests': self.environment.string('FLAKY_TESTS', 'run'),
            "config_flags": self.environment.array('CONFIG_FLAGS', []),
            'v8_test_options': self.environment.array('V8_EXTRA_TEST_OPTIONS', []),
            'release_urlbase': self.environment.string('RELEASE_URLBASE', None),
            'codesign_certificate': self.environment.string('CODESIGN_CERT', None),
            'clang_format_start': self.environment.string('CLANG_FORMAT_START', 'HEAD'),
        }

        # Parallel
        parallel = CLI.get('-j')
        self.options['parallel_args'] = [f'-j {parallel}'] if parallel is not None else []

        for key, value in options.items():
            self.options[key] = value

        # Persistent Values
        raw_version = self.persist(
            'raw_version',
            lambda: subprocess.check_output([sys.executable, 'tools/getnodeversion.py']).decode('utf-8').strip(),
        )

        def get_release_version():
            with open('src/node_version.h', 'r') as f:
                for line in f:
                    match = re.search(r'#define NODE_VERSION_IS_RELEASE ([01])', line)
                    if match:
                        return match.group(1) == '1'
                f.close()
            return False
        release = self.persist('release', get_release_version)
        self.options['release'] = release

        def get_npm_version():
            with open('deps/npm/package.json', 'r') as f:
                return json.load(f)['version']
        self.options['npm_version'] = self.persist('npm_version', get_npm_version)

        # Node Versioning

        version = f'v{raw_version}'
        self.options['version'] = version
        self.options['changelog'] = f'doc/changelogs/{raw_version.split(".")[0]}.md'

        match self.get('distribution_type'):
            case 'release':
                self.options['full_version'] = version
            case 'custom':
                custom_tag = self.environment.string('CUSTOMTAG', None)

                assert custom_tag is not None, 'Error: Custom tag required for custom dist'

                self.options['tag'] = custom_tag
                self.options['full_version'] = f'{version}-{self.options["tag"]}'
            case 'nightly' | 'next-nightly':
                date_string = self.environment.string('DATESTRING', None)
                commit = self.environment.string('COMMIT', None)

                assert date_string is not None, 'Error: Datestring required for nightly dist'
                assert commit is not None, 'Error: Commit required for nightly dist'

                self.options['tag'] = f'{self.options["distribution_type"]}{date_string}{commit}'
                self.options['full_version'] = f'{version}-{self.options["tag"]}'
            case _:
                Logger.print('ERROR', '31', 'Invalid distribution type')
                sys.exit(1)

        self.options['platform'] = uname.sysname.lower()
        match uname.machine.lower():
            case 'x86_64':
                self.options['arch'] = 'x64'
            case 'amd64':
                self.options['arch'] = 'x64'
            case 'ppc64':
                self.options['arch'] = 'ppc64'
            case 'ppc':
                self.options['arch'] = 'ppc'
            case 's390x':
                self.options['arch'] = 's390x'
            case 's390':
                self.options['arch'] = 's390'
            case 'os/390':
                self.options['arch'] = 's390x'
            case 'arm64':
                self.options['arch'] = 'arm64'
            case 'aarch64':
                self.options['arch'] = 'arm64'
            case 'powerpc':
                self.options['arch'] = 'ppc64'
            case 'riscv64':
                self.options['arch'] = 'riscv64'
            case _:
                self.options['arch'] = 'x86'

        self.options['v8_arch'] = self.environment.string('V8_ARCH', 'ia32' if self.options['arch'] == 'x86' else self.options['arch'])

        # Node EXE
        extension = '.exe' if sys.platform == 'win32' else ''
        self.options['node_exe'] = self.environment.string('NODE_EXE', f'node{extension}')
        self.options['node'] = self.environment.string('NODE', f'${os.path.join(os.getcwd(), self.options["node_exe"])}')
        self.options['node_g_exe'] = f'node_g{extension}'
        self.options['npm'] = self.environment.string('NPM', f'${os.path.join("deps", "npm", "bin", "npm-cli.js")}')

        # Node Tarball
        self.options['tar_name'] = f'node-{self.options["full_version"]}'
        self.options['tarball'] = f'{self.options["tar_name"]}.tar'
        self.options['binary_name'] = f'{self.options["tar_name"]}-{self.options["platform"]}-{self.options["arch"]}'
        self.options['binary_tar'] = f'{self.options["binary_name"]}.tar'

        # Prerequisites
        self.options['addons_header_dir'] = f'out/{self.options["build_type"]}/addons_headers'
        self.options['addons_headers_prequisites'] = (
            ['tools/install.py', 'config.gypi', 'common.gypi']
            + glob.glob('deps/openssl/config/*.h')
            + glob.glob('deps/openssl/openssl/include/openssl/*.h')
            + glob.glob('deps/uv/include/*.h')
            + glob.glob('deps/uv/include/*/*.h')
            + glob.glob('deps/v8/include/*.h')
            + glob.glob('deps/v8/include/*/*.h')
            + ['deps/zlib/zconf.h',
               'deps/zlib/zlib.h',
               'src/node.h',
               'src/node_api.h',
               'src/js_native_api.h',
               'src/js_native_api_types.h',
               'src/node_api_types.h',
               'src/node_buffer.h',
               'src/node_object_wrap.h',
               'src/node_version.h'
            ]
        )

        self.options['addons_binding_gypfiles'] = anti_glob(
            'test/addons/??_*/binding.gyp',
            glob.glob('test/addons/*/binding.gyp')
        )

        self.options['addons_binding_sources'] = (
            anti_glob('test/addons/??_*/*.cc', glob.glob('test/addons/*/*.cc'))
            + anti_glob('test/addons/??_*/*.h', glob.glob('test/addons/*/*.h'))
        )

        self.options['addons_prerequisites'] = ['test/addons/.headersbuildstamp', 'deps/npm/node_modules/node-gyp/package.json', 'tools/build_addons.py']

        self.options['docbuildstamp_prerequisites'] = ['tools/doc/addon-verify.mjs', 'doc/api/addons.md']
        if (self.options['platform'] == 'aix' or self.options['platform'] == 'os/400'):
            self.options['docbuildstamp_prerequisites'].append(f'out/{self.options["build_type"]}/node.exp')

        self.options['js_native_api_binding_gypfiles'] = anti_glob(
            'test/js-native-api/??_*/binding.gyp',
            glob.glob('test/js-native-api/*/binding.gyp')
        )

        self.options['js_native_api_binding_sources'] = (
            anti_glob('test/js-native-api/??_*/*.c', glob.glob('test/js-native-api/*/*.c'))
            + anti_glob('test/js-native-api/??_*/*.cc', glob.glob('test/js-native-api/*/*.cc'))
            + anti_glob('test/js-native-api/??_*/*.h', glob.glob('test/js-native-api/*/*.h'))
        )

        self.options['node_api_binding_gypfiles'] = anti_glob(
            'test/node-api/??_*/binding.gyp',
            glob.glob('test/node-api/*/binding.gyp')
        )

        self.options['node_api_binding_sources'] = (
            anti_glob('test/node-api/??_*/*.c', glob.glob('test/node-api/*/*.c'))
            + anti_glob('test/node-api/??_*/*.cc', glob.glob('test/node-api/*/*.cc'))
            + anti_glob('test/node-api/??_*/*.h', glob.glob('test/node-api/*/*.h'))
        )

        self.options['benchmark_napi_binding_gypfiles'] = glob.glob('test/benchmark-napi/*/binding.gyp')
        self.options['benchmark_napi_binding_sources'] = (
            glob.glob('test/benchmark-napi/*/*.c')
            + glob.glob('test/benchmark-napi/*/*.cc')
            + glob.glob('test/benchmark-napi/*/*.h')
        )

        # Extended Testing Suites
        self.options['ci_native_suites'] = self.environment.array('CI_NATIVE_SUITES', self.options['native_suites'] + ['benchmark'])
        self.options['ci_js_suites'] = self.environment.array('CI_JS_SUITES', self.options['js_suites'] + ['pummel'])

        # Intl Specific
        if 'DISABLE_V8_I18N' in self.environment.env:
            self.options['v8_build_options'].append('i18nsupport=off')

        # Debug Specific
        if self.options['build_type'] == 'Debug':
            self.options["config_flags"].append('--debug')

        # MacOS Specific
        if self.options['platform'] == 'darwin':
            self.options['gcov_executable'] = 'xcrun llvm-cov gcov'
        self.options['macos_output_directory'] = os.path.join('out', 'macos')
        self.options['pkg'] = f'{self.options["tar_name"]}.pkg'

        # V8 Tap Setup
        tap_v8_path = os.path.join(os.getcwd(), 'v8-tap')
        tap_v8_intl_path = os.path.join(os.getcwd(), 'v8-intl-tap')
        tap_v8_benchmarks_path = os.path.join(os.getcwd(), 'v8-benchmarks-tap')

        if 'ENABLE_CONVERT_V8_JSON_TO_XML' in self.environment.env:
            self.options['tap_v8'] = ['--json-test-results', f'{tap_v8_path}.json', '--slow-tests-cutoff', '1000000']
            self.options['tap_v8_intl'] = ['--json-test-results', f'{tap_v8_intl_path}.json', '--slow-tests-cutoff', '1000000']
            self.options['tap_v8_benchmarks'] = ['--json-test-results', f'{tap_v8_benchmarks_path}.json', '--slow-tests-cutoff', '1000000']
            self.options['convert_to_junit'] = convert_to_junit
        elif 'ENABLE_V8_TAP' in self.environment.env:
            self.options['tap_v8'] = ['--junitout', f'{tap_v8_path}.xml']
            self.options['tap_v8_intl'] = ['--junitout', f'{tap_v8_intl_path}.xml']
            self.options['tap_v8_benchmarks'] = ['--junitout', f'{tap_v8_benchmarks_path}.xml']
            self.options['convert_to_junit'] = lambda x: True # no-op

        # API Docs
        self.options["api_documentation_directories"] = ['out/doc', 'out/doc/api', 'out/doc/api/assets']
        self.options['api_documentation_sources'] = glob.glob('doc/api/*.md')
        self.options['api_documentation_html'] = [os.path.join('out', md.replace('.md', '.html')) for md in self.options['api_documentation_sources']]
        self.options['api_documentation_json'] = [os.path.join('out', md.replace('.md', '.json')) for md in self.options['api_documentation_sources']]
        self.options['api_documentation_assets'] = [f"out/{source.replace('api_assets/', 'api/assets')}" for source in glob.glob("doc/api_assets/*")]
        self.options['link_data_file'] = 'out/doc/api/link_data.json'
        self.options['versions_data_file'] = 'out/doc/api/versions.json'

        # Linting
        lint_markdown_targets = ['doc', 'src', 'lib', 'benchmark', 'test', 'tools/doc', 'tools/icu']
        self.options["lint_markdown_files"] = []
        for target in lint_markdown_targets:
            globbed = glob.glob(f"{target}/**/*.md", recursive=True)
            globbed = [file for file in globbed if f"{os.path.sep}node_modules{os.path.sep}" not in file]
            globbed = [file for file in globbed if f"text{os.path.sep}fixtures{os.path.sep}" not in file]
            self.options["lint_markdown_files"] += globbed

        self.options["lint_javascript_files"] = ['.eslintrc.js', 'benchmark', 'doc', 'lib', 'test', 'tools']

        ## CPP
        self.options['lint_cpp_addon_doc_files'] = glob.glob('test/addons/??_*/*.cc') + glob.glob('test/addons/??_*/*.h')
        self.options['lint_cpp_exclude'] = (
            self.environment.array('LINT_CPP_EXCLUDE', [])
            + ['src/node_root_certs.h', 'src/tracing/trace_event.h', 'src/tracing/trace_event_common.h']
            + self.options['lint_cpp_addon_doc_files']
        )

        unfiltered_cpp_files = (
            glob.glob("benchmark/napi/*/*.cc")
            + glob.glob("src/*.c")
            + glob.glob("src/*.cc")
            + glob.glob("src/*.h")
            + glob.glob("src/*/*.c")
            + glob.glob("src/*/*.cc")
            + glob.glob("src/*/*.h")
            + glob.glob("test/addons/*/*.cc")
            + glob.glob("test/addons/*/*.h")
            + glob.glob("test/cctest/*.cc")
            + glob.glob("test/cctest/*.h")
            + glob.glob("test/embedding/*.cc")
            + glob.glob("test/embedding/*.h")
            + glob.glob("test/fixtures/*.c")
            + glob.glob("test/js-native-api/*/*.cc")
            + glob.glob("test/node-api/*/*.cc")
            + glob.glob("tools/js2c.cc")
            + glob.glob("tools/icu/*.cc")
            + glob.glob("tools/icu/*.h")
            + glob.glob("tools/code_cache/*.cc")
            + glob.glob("tools/code_cache/*.h")
            + glob.glob("tools/snapshot/*.cc")
            + glob.glob("tools/snapshot/*.h")
        )

        self.options["lint_cpp_files"] = [file for file in unfiltered_cpp_files if file not in self.options["lint_cpp_exclude"]]

        self.options["format_cpp_files"] = (
            self.environment.array('FORMAT_CPP_FILES', [])
            + self.options["lint_cpp_files"]
            + glob.glob("benchmark/napi/*/*.cc")
            + glob.glob("test/js-native-api/*.h")
            + glob.glob("test/js-native-api/*.c")
            + glob.glob("test/js-native-api/*/*.c")
            + glob.glob("test/js-native-api/*/*.h")
            + glob.glob("test/node-api/*/*.c")
            + glob.glob("test/node-api/*/*.h")
        )

        self.options["addon_doc_lint_flags"] = [
            "-whitespace/ending_newline",
            "-build/header_guard"
        ]

    def get(self, key):
        return self.options[key]

    def set(self, key, value):
        self.options[key] = value

    def new(self, options={}, environ={}):
        env = dict(os.environ)
        env.update(CLI.get_cli_env())
        env.update(environ)
        return BuildOptions(
            env=env,
            options=options,
            persistent_values=self.persistent_values # Prevents re-calculation w/ shells and files
        )

class Builder:
    def make(self, target, allow_remake=False, must_succeed=True, options=None):
        if options is None:
            options = self.options

        if not allow_remake and target in self.made_targets:
            self.logger.debug(f"No need to re-make {target}")
            return 0

        # Prefer exact matches
        if target in self.targets:
            if target not in self.made_targets:
                self.made_targets.append(target)
            exit_code = self.targets[target][0](target, options)
            if must_succeed and exit_code != 0:
                self.logger.error(f"Failed to build {target}")
                sys.exit(exit_code)
            return exit_code
        else:
            for key, value in self.targets.items():
                if fnmatch.fnmatch(key, target):
                    if target not in self.made_targets:
                        self.made_targets.append(target)
                    exit_code = value[0](target, options)
                    if must_succeed and exit_code != 0:
                        self.logger.error(f"Failed to build {target}")
                        sys.exit(exit_code)
                    return exit_code
            exit_code = subprocess.call([
                "make",
                target,
            ])
            if must_succeed and exit_code != 0:
                self.logger.error(f"Failed to build {target}")
                sys.exit(exit_code)
            return exit_code

    def dependencies(self, deps, options, must_make=True):
        for dep in deps:
            code = self.make(dep, options=options)
            if must_make and code != 0:
                self.logger.error(f"Failed to build {dep}")
                sys.exit(1)

    def remove(self, list, only_files=False, only_dirs=False, allow_missing=True):
        for item in list:
            try:
                if os.path.isdir(item) and not only_files:
                    shutil.rmtree(item)
                elif os.path.isfile(item) and not only_dirs:
                    os.remove(item)
            except FileNotFoundError:
                if not allow_missing:
                    self.logger.error(f"Failed to remove {item}")
                    sys.exit(1)

    def exec(self, args, options):
        return subprocess.call(args, env=options.environment.env)

    def exec_output(self, args):
        return subprocess.check_output(args).decode('utf-8').strip()

    def run_build_addons(self, input, output, options):
        self.exec([
            sys.executable,
            os.path.join(os.getcwd(), "tools", "build_addons.py"),
            "--loglevel", options.get('log_level'),
            "--headers-dir", options.get('addons_header_dir'),
            input,
        ], options)

        os.open(output, "w").close()

    def try_node(self, *args):
        try:
            return self.exec_output([self.options.get('node'), *args])
        except FileNotFoundError:
            try:
                return self.exec_output(["node", *args])
            except FileNotFoundError:
                self.logger.error("Error: Node.js not found")
                sys.exit(1)

    def node_has_openssl(self):
        if 'node_has_openssl' in self.dynamic_options:
            return self.dynamic_options['node_has_openssl']

        nho = self.try_node('-p', 'process.versions.openssl != undefined')
        self.dynamic_options['node_has_openssl'] = nho
        return nho

    def is_v8_testing_available(self):
        if 'is_v8_testing_available' in self.dynamic_options:
            ivta = self.dynamic_options['is_v8_testing_available']
        else:
            ivta = os.path.exists(os.path.join(os.getcwd(), "deps", "v8", "tools", "run-tests.py"))
            self.dynamic_options['is_v8_testing_available'] = ivta

        if not ivta:
            self.logger.error("Error: V8 testing not available")
        return ivta

    def node_has_icu(self):
        if 'node_has_icu' in self.dynamic_options:
            return self.dynamic_options['node_has_icu']

        nhi = self.try_node('-p', 'typeof Intl === \'object\'')
        self.dynamic_options['node_has_icu'] = nhi
        return nhi

    def fill_template(self, template_file, outfile, node_version, npm_version):
        with open(template_file, 'r') as f:
            template = f.read()
            template = template.replace('{{NODE_VERSION}}', node_version)
            template = template.replace('{{NPM_VERSION}}', npm_version)
            with open(outfile, 'w') as out:
                out.write(template)
                out.close()
            f.close()

    def __init__(self):
        self.options = BuildOptions(do_init=True)
        log_level = max(self.options.get('log_level'), 4 if self.options.get('build_type') == 'Debug' else 0)
        self.logger = Logger(log_level)
        self.made_targets = []
        self.dynamic_options = {}
        self.targets = {
            "all": [self.target_all, "Build all targets \x1b[3m(default)\x1b[0m"],
            "help": [self.target_help, "Display help"],
            f"{self.options.get('node_exe')}": [self.target_node, "Build node (Release)"],
            'node': [self.target_node, "Build node (Release)"],
            f"{self.options.get('node_g_exe')}": [self.target_node, "Build node_g (Debug)"],
            'node_g': [self.target_node, "Build node_g (Debug)"],
            "out/Makefile": [self.target_out_makefile, "Generate Makefile"],
            "config.gypi": [self.target_config_gypi, "Generate config.gypi"],
            "install": [self.target_install, "Install node"],
            "uninstall": [self.target_install, "Uninstall node"],
            "clean": [self.target_clean, "Clean build artifacts"],
            "testclean": [self.target_testclean, "Clean test artifacts"],
            "distclean": [self.target_distclean, "Clean all build artifacts"],
            "coverage-clean": [self.target_coverage_clean, "Clean coverage artifacts"],
            "coverage-build": [self.target_coverage_build, "Build coverage artifacts"],
            "coverage-build-js": [self.target_coverage_build_js, "Build coverage artifacts for JS"],
            "coverage-test": [self.target_coverage_test, "Run coverage tests"],
            "coverage-report-js": [self.target_coverage_report_js, "Report coverage for JS"],
            "cctest": [self.target_cctest, "Run C++ tests"],
            "list-gtests": [self.target_list_gtests, "List Google Tests"],
            "v8": [self.target_v8, "Build V8"],
            "jstest": [self.target_jstest, "Run JS tests"],
            "tooltest": [self.target_tooltest, "Run tool tests"],
            "coverage-run-js": [self.target_coverage_run_js, "Run JS coverage tests"],
            "test": [self.target_test, "Run all tests"],
            "test-only": [self.target_test_only, "Run all tests without building"],
            "test-cov": [self.target_test_cov, "Run coverage tests"],
            "test-valgrind": [self.target_test_valgrind, "Run valgrind tests"],
            "test-check-deopts": [self.target_test_check_deopts, "Check for deopts"],
            "test/addons/.headersbuildstamp": [self.target_test_addons_headersbuildstamp, "Build Addon Headers (Buildstamp)"],
            "test/addons/.buildstamp": [self.target_test_addons_buildstamp, "Build Addons (Buildstamp)"],
            "test/addons/.docbuildstamp": [self.target_test_addons_docbuildstamp, "Build Addon Docs (Buildstamp)"],
            "build-addons": [self.target_build_addons, "Build Addons"],
            "test/js-native-api/.buildstamp": [self.target_test_js_native_api_buildstamp, "Build JS Native API (Buildstamp)"],
            "build-js-native-api-tests": [self.target_build_js_native_api_tests, "Build JS Native API Tests"],
            "test/node-api/.buildstamp": [self.target_test_node_api_buildstamp, "Build Node API (Buildstamp)"],
            "build-node-api-tests": [self.target_build_node_api_tests, "Build Node API Tests"],
            "test/benchmark-napi/.buildstamp": [self.target_test_benchmark_napi_buildstamp, "Build Benchmark NAPI (Buildstamp)"],
            "test-build": [self.target_test_build, "Build all tests"],
            "test-build-js-native-api": [self.target_test_build_js_native_api, "Build JS Native API Tests"],
            "test-build-node-api": [self.target_test_build_node_api, "Build Node API Tests"],
            "test-all": [self.target_test_all, "Run all tests"],
            "test-all-valgrind": [self.target_test_all_valgrind, "Run all tests with valgrind"],
            "test-all-suites": [self.target_test_all_suites, "Run all tests in suites"],
            "test-ci-native": [self.target_test_ci_native, "Run CI Native Tests"],
            "test-ci-js": [self.target_test_ci_js, "Run CI JS Tests"],
            "test-ci": [self.target_test_ci, "Run CI Tests"],
            "build-ci": [self.target_build_ci, "Build CI Tests"],
            "run-ci": [self.target_run_ci, "Run (and Build) CI Tests"],
            "run-test-release": [self.target_run_test, "Run Release Tests"],
            "run-test-debug": [self.target_run_test, "Run Debug Tests"],
            "test-wpt": [self.target_test_wpt, "Run WPT Tests"],
            "test-wpt-report": [self.target_test_wpt_report, "Report WPT Tests"],
            "test-internet": [self.target_test_system, "Run Internet Tests"],
            "test-tick-processor": [self.target_test_system, "Run Tick Processor Tests"],
            "test-hash-seed": [self.target_test_hash_seed, "Run Hash Seed Tests"],
            "test-doc": [self.target_test_doc, "Run Doc Tests"],
            "test-doc-ci": [self.target_test_doc_ci, "Run Doc CI Tests"],
            "test-known-issue": [self.target_test_system, "Run Known Issue Tests"],
            "test-npm": [self.target_test_npm, "Run NPM Tests"],
            "test-npm-publish": [self.target_test_npm_publish, "Run NPM Publish Tests"],
            "test-js-native-api": [self.target_test_js_native_api, "Run JS Native API Tests"],
            "test-js-native-api-clean": [self.target_test_js_native_api_clean, "Clean JS Native API Tests"],
            "test-node-api": [self.target_test_node_api, "Run Node API Tests"],
            "test-node-api-clean": [self.target_test_node_api_clean, "Clean Node API Tests"],
            "test-addons": [self.target_test_addons, "Run Addon Tests"],
            "test-addons-clean": [self.target_test_addons_clean, "Clean Addon Tests"],
            "test-with-async-hooks": [self.target_test_with_async_hooks, "Run Async Hooks Tests"],
            "test-v8": [self.target_test_v8, "Run V8 Tests"],
            "test-v8-intl": [self.target_test_v8_system, "Run V8 Intl Tests"],
            "test-v8-benchmarks": [self.target_test_v8_system, "Run V8 Benchmarks"],
            "test-v8-updates": [self.target_test_v8_updates, "Run V8 Updates Tests"],
            "test-v8-all": [self.target_test_v8_all, "Run V8 All Tests"],
            "tools/doc/node_modules": [self.target_tools_doc_node_modules, "Install Doc Node Modules"],
            "doc-only": [self.target_doc_only, "Build Docs (Only)"],
            "doc": [self.target_doc, "Build Docs"],
            "out/doc": [self.target_out_doc, "Generate Docs"],
            "out/doc/api": [self.target_out_doc_api, "Generate API Docs"],
            "out/doc/api/assets": [self.target_out_doc_api_assets, "Generate API Docs Assets"],
            "out/doc/api/assets/*": [self.target_out_doc_api_assets_wildcard, "Generate API Docs Asset (Wildcard)"],
            f"{self.options.get('link_data_file')}": [self.target_link_data_file, "Generate Link Data File"],
            'link-data': [self.target_link_data_file, "Generate Link Data"],
            f"{self.options.get('versions_data_file')}": [self.target_versions_data_file, "Generate Versions Data File"],
            'versions-data': [self.target_versions_data_file, "Generate Versions Data"],
            "out/doc/api/*.html": [self.target_out_doc_api_wildcard, "Generate API Docs HTML (Wildcard)"],
            "out/doc/api/*.json": [self.target_out_doc_api_wildcard, "Generate API Docs JSON (Wildcard)"],
            "out/doc/api/all.html": [self.target_out_doc_api_all_html, "Generate API Docs All HTML"],
            "out/doc/api/all.json": [self.target_out_doc_api_all_json, "Generate API Docs All JSON"],
            "out/doc/api/stability": [self.target_out_doc_api_stability, "Generate API Docs Stability"],
            "docopen": [self.target_docopen, "Open Docs"],
            "docserve": [self.target_docserve, "Serve Docs"],
            "docclean": [self.target_docclean, "Clean Docs"],
            "release-only": [self.target_release_only, "Build Release (Only)"],
            f"{self.options.get('pkg')}": [self.target_pkg, "Build Package"],
            'pkg': [self.target_pkg, "Build Package"],
            "corepack-update": [self.target_corepack_update, "Update Corepack"],
            f"{self.options.get('tarball')}": [self.target_tarball, "Build Tarball"],
            'tar': [self.target_tarball, "Build Tarball"],
            "tar-upload": [self.target_tar_upload, "Upload Tarball"],
            "doc-upload": [self.target_doc_upload, "Upload Docs"],
            f"{self.options.get('tarball')}-headers": [self.target_tarball_headers, "Build Tarball Headers"],
            'tar-headers': [self.target_tarball_headers, "Build Tarball Headers"],
            "tar-headers-upload": [self.target_tar_headers_upload, "Upload Tarball Headers"],
            f"{self.options.get('binary_tar')}": [self.target_binary_tar, "Build Binary Tarball"],
            'binary': [self.target_binary_tar, "Build Binary Tarball"],
            "binary-upload": [self.target_binary_upload, "Upload Binary Tarball"],
            'bench-addons-build': [self.target_bench_addons_build, "Build Bench Addons"],
            'bench-addons-clean': [self.target_bench_addons_clean, "Clean Bench Addons"],
            'lint-md-rollup': [self.target_lint_md_rollup, "Lint Markdown Rollup"],
            'lint-md-clean': [self.target_lint_md_clean, "Clean Markdown Lint"],
            'tools/.mdlintstamp': [self.target_tools_mdlintstamp, "Lint Markdown (Buildstamp)"],
            'lint-md': [self.target_lint_md, "Lint Markdown"],
            'format-md': [self.target_format_md, "Format Markdown"],
            'lint-js': [self.target_lint_js, "Lint JavaScript"],
            'lint-js-ci': [self.target_lint_js_ci, "Lint JavaScript CI"],
            'format-cpp-build': [self.target_format_cpp_build, "Build C++ Format"],
            'format-cpp-clean': [self.target_format_cpp_clean, "Clean C++ Format"],
            'format-cpp': [self.target_format_cpp, "Format C++"],
            'tools/.cpplintstamp': [self.target_tools_cpplintstamp, "Lint C++ (Buildstamp)"],
            'lint-cpp': [self.target_tools_cpplintstamp, "Lint C++"],
            'tools/.doclintstamp': [self.target_tools_doclintstamp, "Lint Docs (Buildstamp)"],
            'lint-doc': [self.target_tools_doclintstamp, "Lint Docs"],
            'lint-py-build': [self.target_lint_py_build, "Build Python Lint"],
            'lint-py': [self.target_lint_py, "Lint Python"],
            'lint-yaml-build': [self.target_lint_yaml_build, "Build YAML Lint"],
            'lint-yaml': [self.target_lint_yaml, "Lint YAML"],
            'lint': [self.target_lint, "Lint All"],
            'lint-ci': [self.target_lint_ci, "Lint CI"],
            'lint-clean': [self.target_lint_clean, "Clean Lint"],
            'gen-openssl': [self.target_gen_openssl, "Generate OpenSSL"],
            'verify-buildfile': [self.target_verify_buildfile, "Verify Buildfile"],
        }

    ## Make sure to run this BEFORE pushing any changes to this file
    def target_verify_buildfile(self, target, options):
        global_unfound = False
        for name, value in self.__class__.__dict__.items():
            if name.startswith('target_'):
                found = False
                for _, value in self.targets.items():
                    if value[0].__name__ == name:
                        found = True
                        break
                if not found:
                    global_unfound = True
                    self.logger.error(f"\x1b[1m{name}\x1b[0m not found in targets")
        if global_unfound:
            sys.exit(1)

    def target_all(self, target, options):
        if options.get('build_type') == 'Release':
            self.dependencies([options.get('node_exe')], options=options)
        else:
            self.dependencies([options.get('node_g_exe')], options=options)
        return 0

    def target_help(self, target, options):
        is_debug = options.get('build_type') == 'Debug'
        self.logger.info("Available targets:")
        items = sorted(self.targets.items())
        if is_debug:
            for key, value in items:
                func_name = value[0].__name__
                self.logger.info(f"\x1b[1m{key}\x1b[0m: {value[1]} \x1b[3m({func_name})\x1b[0m")
            self.logger.debug("Environment:")
            for key, value in options.environment.env.items():
                self.logger.debug(f"\x1b[1m{key}\x1b[0m: {value}")
        else:
            for key, value in items:
                self.logger.info(f"\x1b[1m{key}\x1b[0m: {value[1]}")
        return 0

    def target_node(self, target, options):
        if target == 'node': target = options.get('node_exe')
        if target == 'node_g': target = options.get('node_g_exe')

        if (options.get('build_with') == 'make'):
            self.dependencies(['config.gypi', 'out/Makefile'], options=options)

            exit_code = self.exec(['make', '-C', 'out', f'BUILDTYPE={options.get("build_type")}', f'V={options.get("log_level")}'], options)
            if exit_code != 0:
                self.logger.error("Failed to build node")
                return exit_code

            os.remove(target) if os.path.exists(target) else None
            os.symlink(os.path.join(os.getcwd(), 'out', 'Release', options.get('node_exe')), target)
        elif (options.get('build_with') == 'ninja'):
            build_type = 'Debug' if target == options.get('node_g_exe') else 'Release'
            self.dependencies(['config.gypi', f'out/{build_type}/build.ninja'], options=options)

            ninja_args = options.environment.array('NINJA_ARGS', [])
            j_arg = CLI.get('-j')
            if j_arg: ninja_args.append(f'-j{j_arg}')

            exit_code = self.exec([options.get('ninja'), '-C', f'out/{build_type}', *ninja_args], options)
            if exit_code != 0:
                self.logger.error("Failed to build node")
                return exit_code

            os.remove(target) if os.path.exists(target) else None
            os.symlink(os.path.join(os.getcwd(), f'out/{build_type}/{options.get("node_exe")}'), target)
        else:
            self.logger.error("Unknown build tool, must be 'make' or 'ninja'")
            return 2
        return 0

    def target_out_makefile(self, target, options):
        self.dependencies([
            'config.gypi',
            'common.gypi',
            'node.gyp',
            'deps/uv/uv.gyp',
            'deps/llhttp/llhttp.gyp',
            'deps/zlib/zlib.gyp',
            'deps/simdutf/simdutf.gyp',
            'deps/ada/ada.gyp',
            'tools/v8_gypfiles/toolchain.gypi',
            'tools/v8_gypfiles/features.gypi',
            'tools/v8_gypfiles/inspector.gypi',
            'tools/v8_gypfiles/v8.gyp'
        ], options=options)

        return self.exec([
            sys.executable,
            "tools/gyp_node.py",
            "-f", "make",
        ], options)

    def target_config_gypi(self, target, options):
        self.dependencies(['configure', 'configure.py', 'src/node_version.h'], options=options)
        return self.exec([sys.executable, 'configure.py'], options)

    def target_install(self, target, options):
        return self.exec([
            sys.executable,
            target, # install or uninstall
            '--dest-dir', options.get('dest_dir'),
            '--prefix', options.get('prefix'),
        ], options)

    def target_clean(self, target, options):
        self.remove([
            os.path.join('out', 'Makefile'),
            options.get('node_exe'),
            options.get('node_g_exe'),
            os.path.join('out', options.get('build_type'), options.get('node_exe')),
            'node_modules',
            'test.tap',
        ] + glob.glob('out/*.o') + glob.glob('out/*.a') + glob.glob('out/*.d'))
        self.remove([os.path.join('deps', 'icu')], allow_missing=True)

        self.make('testclean', allow_remake=True, must_succeed=False, options=options)
        self.make('test-addons-clean', allow_remake=True, must_succeed=False, options=options)
        self.make('bench-addons-clean', allow_remake=True, must_succeed=False, options=options)

        return 0

    def target_testclean(self, target, options):
        self.remove(glob.glob('test/tmp*') + glob.glob('test/.tmp*'))
        return 0

    def target_distclean(self, target, options):
        self.remove([
            'out',
            'config.gypi',
            'icu_config.gypi',
            'config.mk',
            options.get('node_exe'),
            options.get('node_g_exe'),
            'node_modules',
            os.path.join('deps', 'icu'),
            os.path.join('deps', 'icu-tmp'),
        ] + glob.glob('deps/icu4c*.tgz')
          + glob.glob('deps/icu4c*.zip')
          + glob.glob(f'{options.get("binary_tar")}.*')
          + glob.glob(f'{options.get("tarball")}.*')
        )
        return 0

    def target_coverage_clean(self, target, options):
        out = os.path.join('out', options.get('build_type'), 'obj.target')
        self.remove([
            'node_modules',
            'gcovr',
            os.path.join('coverage', 'tmp')
        ] + glob.glob(f'{out}/*.gcda') + glob.glob(f'{out}/*.gcno'))

        return 0

    def target_coverage_build(self, target, options):
        self.dependencies(['all'])
        self.make('coverage-build-js', allow_remake=True, must_succeed=False, options=options)

        if not os.path.exists('gcovr'):
            self.logger.info('Installing gcovr')
            return self.exec([sys.executable, '-m', 'pip', 'install', '-t', 'gcovr', 'gcovr==4.2'], options)

        # $(MAKE)? 253
        return 0

    def target_coverage_build_js(self, target, options):
        os.makedirs('node_modules', exist_ok=True)
        if not os.path.exists('node_modules/c8'):
            self.logger.info('Installing c8')
            return self.exec([options.get('node'), os.path.join('deps', 'npm'), 'install', 'c8', '--no-save', '--no-package-lock'], options)
        return 0

    def target_coverage_test(self, target, options):
        self.dependencies(['coverage-build'])

        out = os.path.join('out', options.get('build_type'), 'obj.target')
        self.remove(glob.glob(f'{out}/*.gcda') + glob.glob(f'{out}/*.gcno'))

        self.make(
            options.get('coverage_tests'),
            allow_remake=True,
            must_succeed=False,
            options=options.new(
                environ={'NODE_V8_COVERAGE': 'coverage/tmp'},
                options={'test_ci_args': options.get('test_ci_args') + ['--type=coverage']}
            )
        )

        self.make('coverage-build-js', allow_remake=True, must_succeed=False, options=options)

        os.chdir(os.path.join(os.getcwd(), "out"))
        pythonpath_env = dict(self.environment.env)
        pythonpath_env["PYTHONPATH"] = os.path.join(os.getcwd(), "..", "gcovr")
        subprocess.call([
            sys.executable,
            "-m", "gcovr",
            "--gcov-exclude='.*\b(deps|usr|out|cctest|embedding)\b'",
            "-v",
            "-r", "../src/",
            "--object-directory", "Release/obj.target",
            "--html",
            "--html-details",
            "-o", "../coverage/cxxcoverage.html",
            f"--gcov-executable={self.options['gcov_executable']}",
        ], env=pythonpath_env)
        os.chdir(os.path.join(os.getcwd(), ".."))

        self.logger.info(r"Javascript Coverage \%\%: ")
        with open("coverage/index.html", "r") as f:
                lines = f.readlines()
                for line in lines:
                    if "Lines" in line:
                        found_line = lines[lines.index(line) - 1]
                        self.logger.info(re.sub('s/<[^>]*>//g', '', found_line).strip())
                        break
                f.close()

        self.logger.info(r"C++ Coverage \%\%: ")
        with open("coverage/cxxcoverage.html", "r") as f:
            lines = f.readlines()
            for line in lines:
                if "Lines" in line:
                    context_lines = lines[lines.index(line) - 3:lines.index(line)]
                    for context_line in context_lines:
                        if "style" in context_line:
                            found_line = context_line
                            break
                    self.logger.info(re.sub('s/<[^>]*>//g', '', found_line).strip())
                    break
            f.close()

        return 0

    def target_coverage_report_js(self, target, options):
        self.make('coverage-build-js', allow_remake=True, must_succeed=False, options=options)
        return self.exec([
            options.get('node'),
            os.path.join('deps', 'c8', 'bin', 'c8'),
            'report',
        ], options)

    def target_cctest(self, target, options):
        self.dependencies(['all'], options=options)
        exit = self.exec([
            f"out/{options.get('build_type')}/node",
            "--gtest_filter=" + options.get('gtest_filter'),
        ], options)

        if exit != 0: return exit

        return self.exec([
            options.get('node'),
            os.path.join('test', 'embedding', 'test-embedding.js'),
        ], options)

    def target_list_gtests(self, target, options):
        if not os.path.exists(os.path.join(os.getcwd(), "out", options.get('build_type'), "cctest")):
            self.logger.error("cctest not found, run 'cctest'")
            return 1

        return self.exec([
            os.path.join(os.getcwd(), "out", options.get('build_type'), "cctest"),
            "--gtest_list_tests"
        ], options)

    def target_v8(self, target, options):
        return self.exec([
            os.path.join('tools', 'make-v8.sh'),
            f"{options.get('v8_arch')}.{options.get('build_type').lower()}",
        ] + options.get('v8_build_options'), options)

    def target_jstest(self, target, options):
        self.dependencies(['build-addons', 'build-js-native-api-tests', 'build-node-api-tests'], options=options)
        return self.exec([
            sys.executable,
            os.path.join(os.getcwd(), "tools", "test.py"),
        ] + options.get('parallel_args') + [
            f"--skip-tests={','.join(options.get('ci_skip_tests'))}",
        ] + options.get('test_ci_args') + options.get('js_suites') + options.get('native_suites'), options)

    def target_tooltest(self, target, options):
        return self.exec([
            sys.executable,
            '-m', 'unittest',
            'discover',
            '-s', os.path.join('test', 'tools'),
        ], options)

    def target_coverage_run_js(self, target, options):
        self.remove(['coverage/tmp'])

        self.make(
            target='jstest',
            allow_remake=True,
            must_succeed=False,
            options=options.new(
                environ={
                    'NODE_V8_COVERAGE': 'coverage/tmp',
                },
                options={
                    'ci_skip_tests': options.get('coverage_skip_tests'),
                    'test_ci_args': options.get('test_ci_args') + ['--type=coverage']
                }
            )
        )

        return self.make('coverage-report-js', allow_remake=True, must_succeed=False, options=options)

    def target_test(self, target, options):
        self.dependencies(['all'], options=options)

        makes = ['tooltest', 'test-doc', 'build-addons', 'build-js-native-api-tests', 'build-node-api-tests', 'cctest', 'jstest']
        for make in makes:
            self.make(make, allow_remake=True, must_succeed=False, options=options)
        return 0

    def target_test_only(self, target, options):
        self.dependencies(['all'], options=options)

        makes = ['build-addons', 'build-js-native-api-tests', 'build-node-api-tests', 'cctest', 'jstest', 'tooltest']
        for make in makes:
            self.make(make, allow_remake=True, must_succeed=False, options=options)
        return 0

    def target_test_cov(self, target, options):
        self.dependencies(['all'], options=options)

        makes = ['build-addons', 'build-js-native-api-tests', 'build-node-api-tests', 'cctest']
        for make in makes:
            self.make(make, allow_remake=True, must_succeed=False, options=options)

        return self.make(
            target='jstest',
            allow_remake=True,
            must_succeed=False,
            options=options.new(
                options={
                    'ci_skip_tests': options.get('coverage_skip_tests'),
                }
            )
        )

    def target_test_valgrind(self, target, options):
        self.dependencies(['all'], options=options)
        return self.exec([
            sys.executable,
            os.path.join('tools', 'test.py'),
        ] + options.get('parallel_args') + [
            f'--mode={options.get("build_type").lower()}',
            '--valgrind', 'sequential', 'parallel', 'message',
        ], options)

    def target_test_check_deopts(self, target, options):
        self.dependencies(['all'], options=options)
        return self.exec([
            sys.executable,
            os.path.join('tools', 'test.py'),
        ] + options.get('parallel_args') + [
            f'--mode={options.get("build_type").lower()}',
            '--check-deopts', 'parallel', 'message',
        ], options)

    def target_test_addons_headersbuildstamp(self, target, options):
        self.dependencies(options.get('addons_headers_prerequisities'), options=options)
        self.exec([
            sys.executable,
            os.path.join('tools', 'install.py'),
            'install',
            '--headers-only',
            '--dest-dir', options.get('addons_header_dir'),
            '--prefix', '/'
        ])

        os.open(target, "w").close()
        return 0

    def target_test_addons_buildstamp(self, target, options):
        self.dependencies(
            options.get('addons_prerequisities')
            + options.get('addons_binding_gypfiles')
            + options.get('addons_binding_sources')
        , options=options)

        self.run_build_addons(os.path.join(os.getcwd(), 'test', 'addons'), target, options)
        return 0

    def target_test_addons_docbuildstamp(self, target, options):
        self.dependencies(options.get('docbuildstamp_prerequisites') + ['tools/doc/node_modules'], options=options)
        if not self.node_has_openssl():
            self.logger.warn('Skipping doc build, missing openssl')
            return 0

        self.remove(glob.glob('test/addons/??_*/'))

        try:
            self.try_node(options.get('docbuildstamp_prerequisites')[0])
        except:
            return 0

        os.open(target, "w").close()
        return 0

    def target_build_addons(self, target, options):
        self.dependencies([
            'test/addons/.headersbuildstamp',
            'test/addons/.buildstamp',
        ], must_make=False, options=options)
        return 0

    def target_test_js_native_api_buildstamp(self, target, options):
        self.dependencies(
            options.get('addons_prerequisites')
            + options.get('js_native_api_binding_gypfiles')
            + options.get('js_native_api_binding_sources')
            + ['src/js_native_api_v8.h', 'src/js_native_api_v8_internals.h']
        , options=options)

        self.run_build_addons(os.path.join(os.getcwd(), 'test', 'js-native-api'), target, options)
        return 0

    def target_build_js_native_api_tests(self, target, options):
        self.dependencies([
            self.options.get('node_exe'),
            'test/js-native-api/.buildstamp',
        ], must_make=False, options=options)

        return 0

    def target_test_node_api_buildstamp(self, target, options):
        self.dependencies(
            options.get('addons_prerequisites')
            + options.get('node_api_binding_gypfiles')
            + options.get('node_api_binding_sources')
            + ['src/node_api.h', 'src/node_api_types.h']
        , options=options)

        self.run_build_addons(os.path.join(os.getcwd(), 'test', 'node-api'), target, options)
        return 0

    def target_build_node_api_tests(self, target, options):
        self.dependencies([
            self.options.get('node_exe'),
            'test/node-api/.buildstamp',
        ], must_make=False, options=options)

        return 0

    def target_test_benchmark_napi_buildstamp(self, target, options):
        self.dependencies(
            options.get('addons_prerequisites')
            + options.get('benchmark_napi_binding_gypfiles')
            + options.get('benchmark_napi_binding_sources')
        , options=options)

        self.run_build_addons(os.path.join(os.getcwd(), 'test', 'benchmark-napi'), target, options)
        return 0

    def target_test_build(self, target, options):
        self.dependencies(['all', 'build-addons', 'build-js-native-api-tests', 'build-node-api-tests'], must_make=False, options=options)
        return 0

    def target_test_build_js_native_api(self, target, options):
        self.dependencies(['all', 'build-js-native-api-tests'], options=options)
        return 0

    def target_test_build_node_api(self, target, options):
        self.dependencies(['all', 'build-node-api-tests'], options=options)
        return 0

    def target_test_all(self, target, options):
        self.dependencies(['test-build'], options=options)
        return self.exec([
            sys.executable,
            os.path.join('tools', 'test.py'),
        ] + options.get('parallel_args') + [
            '--mode=debug,release',
        ], options)

    def target_test_all_valgrind(self, target, options):
        self.dependencies(['test-build'], options=options)
        return self.exec([
            sys.executable,
            os.path.join('tools', 'test.py'),
        ] + options.get('parallel_args') + [
            '--mode=debug,release',
            '--valgrind',
        ], options)

    def target_test_all_suites(self, target, options):
        self.dependencies(['test-build', 'bench-addons-build', 'doc-only'], must_make=False, options=options)
        return self.exec([
            sys.executable,
            os.path.join('tools', 'test.py'),
        ] + options.get('parallel_args') + [
            f'--mode={options.get("build_type").lower()}',
            'test/*',
        ], options)

    def target_test_ci_native(self, target, options):
        custom_options = { 'LOGLEVEL': 'info' }
        self.dependencies([
            'benchmark/napi/.buildstamp',
            'test/addons/.buildstamp',
            'test/js-native-api/.buildstamp',
            'test/node-api/.buildstamp',
        ], must_make=False, options=options)

        return self.exec([
            sys.executable,
            os.path.join('tools', 'test.py'),
        ] + options.get('parallel_args') + [
            '-p', 'tap',
            '--logfile', 'test.tap',
            f'--mode={options.get("build_type").lower()}',
            f'--flaky-tests={options.get("flaky_tests")}',
        ] + options.get('test_ci_args') + options.get('ci_native_suites'), options.new(environ=custom_options))

    def target_test_ci_js(self, target, options):
        return self.exec([
            sys.executable,
            os.path.join('tools', 'test.py'),
        ] + options.get('parallel_args') + [
            '-p', 'tap',
            '--logfile', 'test.tap',
            f'--mode={options.get("build_type").lower()}',
            f'--flaky-tests={options.get("flaky_tests")}',
        ] + options.get('test_ci_args') + options.get('ci_js_suites'), options)

    def target_test_ci(self, target, options):
        custom_options = { 'LOGLEVEL': 'info' }
        self.dependencies([
            'bench-addons-build',
            'build-addons',
            'build-js-native-api-tests',
            'build-node-api-tests',
            'doc-only',
        ], must_make=False, options=options)

        ci_doc = ['doctool'] if self.node_has_openssl() else []

        self.exec([
            sys.executable,
            os.path.join('tools', 'test.py'),
        ] + options.get('parallel_args') + [
            '-p', 'tap',
            '--logfile', 'test.tap',
            f'--mode={options.get("build_type").lower()}',
            f'--flaky-tests={options.get("flaky_tests")}',
        ] + options.get('test_ci_args') + options.get('ci_native_suites') + options.get('ci_js_suites') + [ci_doc], options.new(environ=custom_options))

        return self.exec([options.get('node', os.path.join('test', 'embedding', 'test-embedding.js'))], options)

    def target_build_ci(self, target, options):
        return self.exec([
            sys.executable,
            './configure',
            '--verbose',
        ] + options.get('config_flags'), options)
        # $(MAKE)? 581

    def target_run_ci(self, target, options):
        self.dependencies(['build-ci'], options=options)
        return self.make('test-ci', allow_remake=True, must_succeed=False, options=options) # -j1

    def target_run_test(self, target, options):
        match = re.match(r"run-test-(\w+)", target)
        mode = match.group(1)
        return self.exec(sys.executable, os.path.join('tools', 'test.py'), f'--mode={mode}')

    def target_test_wpt(self, target, options):
        self.dependencies(['all'], options=options)
        return self.exec([
            sys.executable,
            os.path.join('tools', 'test.py'),
            '--wpt',
        ], options)

    def target_test_wpt_report(self, target, options):
        self.remove('out/wpt')
        os.makedirs('out/wpt')
        self.exec([
            sys.executable,
            os.path.join('tools', 'test.py'),
            '--shell', options.get('node'),
        ] + options.get('parallel_args') + [
            'wpt'
        ], options.new(environ={'WPT_REPORT': '1'}))

        return self.exec([
            options.get('node'),
            os.path.join('tools', 'merge-wpt-reports.mjs'),
        ])

    def target_test_system(self, target, options):
        self.dependencies(['all'], options=options)
        # test-<system>
        match = re.match(r"test-([^\s]+)", target)
        system = match.group(1)
        parallel = options.get('parallel_args')
        if match == 'known-issues':
            parallel = []
        return self.exec([
            sys.executable,
            os.path.join('tools', 'test.py'),
        ] + parallel + [
            system,
        ], options)

    def target_test_hash_seed(self, target, options):
        self.dependencies(['all'], options=options)
        return self.exec([
            options.get('node'),
            os.path.join(os.getcwd(), 'test', 'pummel', 'test-hash-seed.js'),
        ], options)

    def target_test_doc(self, target, options):
        self.dependencies(['doc-only', 'lint-md'], options=options)
        if self.node_has_openssl():
            return self.exec([
                sys.executable,
                os.path.join('tools', 'test.py'),
            ] + options.get('parallel_args') + [
                'doctool',
            ])
        else:
            self.logger.warn('Skipping doc tests, missing openssl')
            return 0

    def target_test_doc_ci(self, target, options):
        self.dependencies(['doc-only'], options=options)
        return self.exec([
            sys.executable,
            os.path.join('tools', 'test.py'),
        ] + options.get('parallel_args') + options.get('test_ci_args') + ['doctool'], options)

    def target_test_npm(self, target, options):
        self.dependencies([options.get('node_exe')], options=options)
        return self.exec([
            options.get('node'),
            os.path.join(os.getcwd(), "tools", "test-npm-package"),
            '--install', '--logfile=test-npm.tap',
            'deps/npm', 'test'
        ], options)

    def target_test_npm_publish(self, target, options):
        self.dependencies([options.get('node_exe')], options=options)
        return self.exec([
            options.get('node'),
            os.path.join(os.getcwd(), "deps", "npm", "test", "run.js")
        ], options.new(environ={'npm_package_config_publishtest': 'true'}))

    def target_test_js_native_api(self, target, options):
        self.dependencies(['test-build-js-native-api-tests'], options=options)
        return self.exec([
            sys.executable,
            os.path.join('tools', 'test.py'),
        ] + options.get('parallel_args') + [
            f'--mode={options.get("build_type").lower()}',
            'js-native-api',
        ], options)

    def target_test_js_native_api_clean(self, target, options):
        self.remove(glob.glob('test/js-native-api/*/build') + ["test/js-native-api/.buildstamp"])
        return 0

    def target_test_node_api(self, target, options):
        self.dependencies(['test-build-node-api-tests'], options=options)
        return self.exec([
            sys.executable,
            os.path.join('tools', 'test.py'),
        ] + options.get('parallel_args') + [
            f'--mode={options.get("build_type").lower()}',
            'node-api',
        ], options)

    def target_test_node_api_clean(self, target, options):
        self.remove(glob.glob('test/node-api/*/build') + ["test/node-api/.buildstamp"])
        return 0

    def target_test_addons(self, target, options):
        self.dependencies(['test-build', 'test-js-native-api', 'test-node-api'], options=options)
        return self.exec([
            sys.executable,
            os.path.join('tools', 'test.py'),
        ] + options.get('parallel_args') + [
            f'--mode={options.get("build_type").lower()}',
            'addons',
        ], options)

    def target_test_addons_clean(self, target, options):
        self.remove(
            [options.get('addons_header_dir'), 'test/addons/.buildstamp', 'test/addons/.headersbuildstamp', 'test/addons/.docbuildstamp']
            + glob.glob('test/addons/*/build')
            + glob.glob('test/addons/??_*/')
        )

        self.make('test-js-native-api-clean', allow_remake=True, must_succeed=False, options=options)
        return self.make('test-node-api-clean', allow_remake=True, must_succeed=False, options=options)

    def target_test_with_async_hooks(self, target, options):
        self.dependencies(['build-addons', 'build-js-native-api-tests', 'build-node-api-tests', 'cctest'], options=options)

        return self.exec([
            sys.executable,
            os.path.join('tools', 'test.py'),
        ] + options.get('parallel_args') + [
            f'--mode={options.get("build_type").lower()}',
        ] + options.get('js_suites') + options.get('native_suites'), options=options.new(environ={'NODE_TEST_WITH_ASYNC_HOOKS': '1'}))

    def target_test_v8(self, target, options):
        self.dependencies(['v8'], options=options)

        if not self.is_v8_testing_available(): return 1

        self.exec([
            sys.executable,
            os.path.join(os.getcwd(), "deps", "v8", "tools", "run-tests.py"),
            "--gn",
            f"--arch={options.get('v8_arch')}",
        ] + options.get('v8_test_options') + [
            "mjsunit",
            "cctest",
            "debugger",
            "inspector",
            "message",
            "preparser",
        ] + options.get('tap_v8'), options)

        return self.make('test-hash-seed', allow_remake=True, must_succeed=False, options=options)

    def target_test_v8_system(self, target, options):
        self.dependencies(['v8'], options=options)

        if not self.is_v8_testing_available(): return 1

        match = re.match(r"test-v8-(\w+)", target)
        system = match.group(1)

        self.exec([
            sys.executable,
            os.path.join(os.getcwd(), "deps", "v8", "tools", "run-tests.py"),
            "--gn",
            f"--arch={options.get('v8_arch')}",
            system,
        ] + options.get('tap_v8_' + system), options)

        convert_to_junit(options.get('tap_v8_' + system + '_json'))
        return 0

    def target_test_v8_updates(self, target, options):
        return self.exec([
            sys.executable,
            os.path.join(os.getcwd(), "deps", "test.py"),
        ] + options.get('parallel_args') + [
            f"--mode={options.get('build_type').lower()}",
            'v8-updates',
        ], options)

    def target_test_v8_all(self, target, options):
        self.dependencies(['test-v8', 'test-v8-intl', 'test-v8-benchmarks', 'test-v8-updates'], options=options)
        return 0

    def target_tools_doc_node_modules(self, target, options):
        if not self.node_has_openssl():
            self.logger.warn('Skipping installing doc node modules, missing openssl')
            return 0
        os.chdir(os.path.join(os.getcwd(), 'tools', 'doc'))
        self.try_node([
            os.path.join(os.getcwd(), self.options["NPM"]),
            "ci",
            "--no-package-lock"
        ])
        os.chdir(os.path.join(os.getcwd(), "..", ".."))
        return 0

    def target_doc_only(self, target, options):
        self.dependencies(['tools/doc/node_modules'] + options.get('api_documentation_directories') + options.get('api_documentation_assets'), options=options)

        if not self.node_has_openssl():
            self.logger.warn('Skipping doc build, missing openssl')
            return 0

        self.make('out/doc/api/all.html', allow_remake=True, must_succeed=False, options=options)
        self.make('out/doc/api/all.json', allow_remake=True, must_succeed=False, options=options)
        self.make('out/doc/api/stability', allow_remake=True, must_succeed=False, options=options)
        return 0

    def target_doc(self, target, options):
        self.dependencies([options.get('node_exe'), 'doc-only'], options=options)
        return 0

    def target_out_doc(self, target, options):
        os.makedirs(os.path.join('out', 'doc'), exist_ok=True)
        return 0

    def target_out_doc_api(self, target, options):
        os.makedirs(os.path.join('out', 'doc', 'api'), exist_ok=True)
        shutil.copytree(
            os.path.join(os.getcwd(), 'doc', 'api'),
            os.path.join(os.getcwd(), 'out', 'doc', 'api'),
            dirs_exist_ok=True
        )
        return 0

    def target_out_doc_api_assets(self, target, options):
        os.makedirs(os.path.join('out', 'doc', 'api', 'assets'), exist_ok=True)

        if os.path.exist(os.path.join('doc', 'api', 'assets')):
            shutil.copytree(
                os.path.join(os.getcwd(), 'doc', 'api', 'assets'),
                os.path.join(os.getcwd(), 'out', 'doc', 'api', 'assets'),
                dirs_exist_ok=True
            )
        return 0

    def target_out_doc_api_assets_wildcard(self, target, options):
        filename = os.path.basename(target)
        self.dependencies(['out/doc/api/assets', f'doc/api_assets/{filename}'], options=options)

        shutil.copy(
            os.path.join(os.getcwd(), 'doc', 'api_assets', filename),
            os.path.join(os.getcwd(), 'out', 'doc', 'api', 'assets', filename)
        )

        self.remove(['out/doc/api/assets/README.md'])
        return 0

    def target_link_data_file(self, target, options):
        lib = glob.glob('lib/*.js')
        self.dependencies(['out/doc'] + lib + ['tools/doc/apilinks.mjs'], options=options)
        self.try_node(os.path.join('tools', 'doc', 'apilinks.mjs'), target, *lib)
        return 0

    def target_versions_data_file(self, target, options):
        self.dependencies(['CHANGELOG.md', 'src/node_version.h', 'tools/doc/versions.mjs'], options=options)
        self.try_node(os.path.join('tools', 'doc', 'versions.mjs'), target)
        return 0

    def target_out_doc_api_wildcard(self, target, options):
        filename = os.path.basename(target)
        self.dependencies([
            options.get('link_data_file'),
            'out/doc/api',
            f'doc/api/{filename.replace(".html", ".md")}',
            'tools/doc/generate.mjs',
            'tools/doc/markdown.mjs',
            'tools/doc/html.mjs',
            'tools/doc/json.mjs',
            'tools/doc/apilinks.mjs',
            options.get('versions_data_file'),
        ], options=options)

        if not self.node_has_icu():
            self.logger.warn('Skipping doc build, missing icu')
            return 0

        self.try_node(
            os.path.join('tools', 'doc', 'generate.mjs'),
            f'--node-version={options.get("full_version")}',
            f'--apilinks={options.get("link_data_file")}',
            f'doc/api/{filename.replace(".html", ".md")}',
            '--output-directory=out/doc/api',
            f'--versions-file={options.get("versions_data_file")}',
        )

        return 0

    def target_out_doc_api_all_html(self, target, options):
        self.dependencies(['out/doc/api'] + options.get('api_documentation_html') + ['tools/doc/allhtml.mjs', 'tools/doc/apilinks.mjs'], options=options)

        if not self.node_has_icu():
            self.logger.warn('Skipping doc build, missing icu')
            return 0

        self.try_node(os.path.join('tools', 'doc', 'allhtml.mjs'))
        return 0

    def target_out_doc_api_all_json(self, target, options):
        self.dependencies(['out/doc/api'] + options.get('api_documentation_json') + ['tools/doc/alljson.mjs'], options=options)

        if not self.node_has_icu():
            self.logger.warn('Skipping doc build, missing icu')
            return 0

        self.try_node(os.path.join('tools', 'doc', 'alljson.mjs'))
        return 0

    def target_out_doc_api_stability(self, target, options):
        self.dependencies(['out/doc/api', 'out/doc/api/all.json', 'tools/doc/stability.mjs'], options=options)

        if not self.node_has_icu():
            self.logger.warn('Skipping doc build, missing icu')
            return 0

        self.try_node(os.path.join('tools', 'doc', 'stability.mjs'))
        return 0

    def target_docopen(self, target, options):
        self.dependencies(['out/doc/api/all.html'], options=options)
        return self.exec([sys.executable, '-m', 'webbrowser', pathlib.Path(os.getcwd(), 'out', 'doc', 'api', 'all.html').as_uri()])

    def target_docserve(self, target, options):
        self.dependencies(options.get('api_documentation_html') + options.get('api_documentation_assets'), options=options)
        return self.exec([sys.executable, '-m', 'http.server', '8000', '--bind', '127.0.0.1', '--directory', 'out/doc/api'])

    def target_docclean(self, target, options):
        self.remove(['out/doc', self.options['versions_data_file']])
        return 0

    def target_release_only(self, target, options):
        dist_type = options.get("distribution_type")
        release = options.get("release")

        if dist_type == "release":
            for file in glob.glob("doc/api/*.md"):
                with open(file, "r") as f:
                    if "REPLACEME" in f.read():
                        f.close()
                        self.logger.error(f"REPLACEME present in {file}")
                        return 1
                    f.close()
            with open("doc/api/deprecations.md", "r") as f:
                if "DEP...X" in f.read():
                    f.close()
                    self.logger.error("DEP...X present in doc/api/deprecations.md")
                    return 1
                f.close()
            if not release:
                self.logger.error("#NODE_VERSION_IS_RELEASE is not set to 1")
                return 1

        porcelian = self.exec(["git", "status", "--porcelain"])
        if any(line.startswith("??") for line in porcelian.splitlines()):
            self.logger.error("The git repository is not clean")
            return 1

        if not release and not os.path.exists(options.get("changelog")):
            Logger.error("Error: #NODE_VERSION_IS_RELEASE's CHANGELOG.md does not exist")
            return 1

        return 0

    def make_pkg(self, cpu, options, out_dir, environ={}):
        if cpu == 'x64':
            prefix_dir = os.path.join(out_dir, 'dist', 'x64', 'node')
        else:
            prefix_dir = os.path.join(out_dir, 'dist', 'node')

        self.exec([
            sys.executable,
            './configure',
            f'--dest-cpu={cpu}',
            f'--tag={options.get("tag")}',
            f'--release-urlbase={options.get("release_urlbase")}',
        ] + options.get('config_flags') + options.get('build_release_flags'), options=options.new(
            environ=environ
        ))

        self.make('install', allow_remake=True, must_succeed=False, options=options.new(environ={'DESTDIR': os.path.join(prefix_dir)}))

        return self.exec(['sh', 'tools/osx-codesign.sh'], options=options.new(
            environ={
                'SIGN': options.get('codesign_certificate'),
                'PKGDIR': os.path.join(prefix_dir, 'usr', 'local'),
            }
        ))

    def target_pkg(self, target, options):
        self.dependencies(['release-only'],options=options)

        if options.get('platform') != 'darwin' or options.get('arch') != 'arm64':
            self.logger.error("Only macOS arm64 is supported")
            return 1

        out_dir = options.get('macos_output_directory')
        full_version = options.get('full_version')
        npm_version = options.get('npm_version')

        self.remove([out_dir])
        os.makedirs(os.path.join(out_dir, 'installer', 'productbuild'), exist_ok=True)

        self.fill_template(
            os.path.join('tools', 'macos-installer', 'productbuild', 'distribution.xml.tmpl'),
            os.path.join(out_dir, 'installer', 'productbuild', 'distribution.xml'),
            full_version,
            npm_version
        )

        for directory in glob.glob("tools/macos-installer/productbuild/Resources/*/"):
            lang = os.path.basename(directory)
            os.makedirs(os.path.join(out_dir, 'installer', 'productbuild', 'Resources', lang), exist_ok=True)

            self._fill_template(
                os.path.join('tools', 'macos-installer', 'productbuild', 'Resources', lang, 'welcome.html.tmpl'),
                os.path.join(out_dir, 'installer', 'productbuild', 'Resources', lang, 'welcome.html'),
                full_version,
                npm_version
            )

            self._fill_template(
                os.path.join('tools', 'macos-installer', 'productbuild', 'Resources', lang, 'conclusion.html.tmpl'),
                os.path.join(out_dir, 'installer', 'productbuild', 'Resources', lang, 'conclusion.html'),
                full_version,
                npm_version
            )

        CC = "cc -arch x86_64"
        CXX = "c++ -arch x86_64"
        self.make_pkg('x64', options, out_dir, environ={
            'CC': CC,
            'CXX': CXX,
            'CC_host': CC,
            'CXX_host': CXX,
            'CC_target': CC,
            'CXX_target': CXX,
        })
        self.make_pkg('arm64', options, out_dir)

        self.exec([
            "lipo",
            os.path.join(out_dir, 'dist', 'x64', 'node', 'usr', 'local', 'bin', 'node'),
            os.path.join(out_dir, 'dist', 'node', 'usr', 'local', 'bin', 'node'),
            "-output",
            os.path.join(out_dir, 'dist', 'node', 'usr', 'local', 'bin', 'node'),
            "-create"
        ], options)

        os.makedirs(os.path.join(out_dir, 'dist', 'npm', 'usr', 'local', 'lib', 'node_modules'), exist_ok=True)
        os.makedirs(os.path.join(out_dir, 'pkg'), exist_ok=True)

        shutil.copytree(
            os.path.join(out_dir, 'dist', 'node', 'usr', 'local', 'lib', 'node_modules', 'npm'),
            os.path.join(out_dir, 'dist', 'npm', 'usr', 'local', 'lib', 'node_modules', 'npm'),
            dirs_exist_ok=True
        )

        os.unlink(os.path.join(out_dir, 'dist', 'npm', 'usr', 'local', 'lib', 'node_modules', 'npm', 'bin', 'npm'))
        os.unlink(os.path.join(out_dir, 'dist', 'npm', 'usr', 'local', 'lib', 'node_modules', 'npm', 'bin', 'npx'))

        self.exec([options.get('node'), 'tools/license2rtf.js', '<', 'LICENSE.md', '>', os.path.join(out_dir, 'installer', 'productbuild', 'Resources', 'License.rtf')])

        shutil.copy(
            os.path.join('doc', 'osx_installer_logo.png'),
            os.path.join(out_dir, 'installer', 'productbuild', 'Resources', 'osx_installer_logo.png')
        )

        self.exec([
            "pkgbuild",
            "--version", full_version,
            "--identifier", "org.nodejs.node.pkg",
            '--root', os.path.join(out_dir, 'dist', 'node'),
            os.path.join(out_dir, 'pkgs', f'node-{full_version}.pkg'),
        ], options)

        self.exec([
            "pkgbuild",
            "--version", npm_version,
            "--identifier", "org.nodejs.npm.pkg",
            '--root', os.path.join(out_dir, 'dist', 'npm'),
            '--scripts', os.path.join('tools', 'macos-installer', 'pkgbuild', 'npm', 'scripts'),
            os.path.join(out_dir, 'pkgs', f'npm-{npm_version}.pkg'),
        ], options)

        self.exec([
            "productbuild",
            '--distribution', os.path.join(out_dir, 'installer', 'productbuild', 'distribution.xml'),
            '--resources', os.path.join(out_dir, 'installer', 'productbuild', 'Resources'),
            '--package-path', os.path.join(out_dir, 'pkgs'), f'./{options.get("pkg")}'
        ], options)

        self.exec(['sh', os.path.join('tools', 'osx-productsign.sh')], options=options.new(environ={
            'SIGN': options.get('codesign_certificate'),
            'PKG': options.get('pkg'),
        }))

        return self.exec([
            "sh", "tools/osx-notarize.sh",
            full_version
        ], options)

    def target_corepack_update(self, target, options):
        if not HAS_REQUESTS:
            self.logger.error("requests is required to update corepack")
            return 1
        
        temp_dir = tempfile.mkdtemp()
        url = self.exec_output(['npm', 'view', 'corepack', 'dist.tarball'])

        response = requests.get(url)
        with open(os.path.join(temp_dir, 'corepack.tgz'), 'wb') as f:
            f.write(response.content)
            f.close()

        with tarfile.open(os.path.join(temp_dir, 'corepack.tgz')) as tar:
            tar.extractall(temp_dir)

        shutil.move(os.path.join(temp_dir, 'package'), os.path.join('deps', 'corepack'))

        self.remove(temp_dir)

        for file in glob.glob('deps/corepack/shims/*'):
            os.chmod(file, 0o755)

        return 0

    def target_tarball(self, target, options):
        self.dependencies(['release-only', 'doc-only'], options=options)

        tarname = options.get('tar_name')
        self.exec(['git', 'checkout-index', '-a', '-f', f'--prefix={tarname}/'])
        shutil.copyfile(os.path.join('doc', 'node.1'), os.path.join(tarname, 'doc', 'api', 'node.1'))
        shutil.copytree(os.path.join('out', 'doc', 'api'), os.path.join(tarname, 'doc', 'api'), dirs_exist_ok=True)

        self.remove([
            os.path.join(tarname, '.editorconfig'),
            os.path.join(tarname, '.mailmap'),
            os.path.join(tarname, 'deps', 'openssl', 'openssl', 'demos'),
            os.path.join(tarname, 'deps', 'openssl', 'openssl', 'doc'),
            os.path.join(tarname, 'deps', 'openssl', 'openssl', 'test'),
            os.path.join(tarname, 'deps', 'uv', 'docs'),
            os.path.join(tarname, 'deps', 'uv', 'samples'),
            os.path.join(tarname, 'deps', 'uv', 'test'),
            os.path.join(tarname, 'deps', 'v8', 'samples'),
            os.path.join(tarname, 'deps', 'v8', 'tools/profviz'),
            os.path.join(tarname, 'deps', 'v8', 'tools/run-test.py'),
            os.path.join(tarname, 'doc', 'images'),
            os.path.join(tarname, 'tools', 'cpplint.py'),
            os.path.join(tarname, 'tools', 'eslint-rules'),
            os.path.join(tarname, 'tools', 'license-builder.sh'),
            os.path.join(tarname, 'tools', 'node_modules'),
            os.path.join(tarname, 'tools', 'osx-pkg.pmdoc')
        ] + (
            glob.glob(f'{tarname}/.git*')
            + glob.glob(f'{tarname}/test*.tap')
            + glob.glob(f'{tarname}/osx-*')
        ))

        self.remove(
            anti_glob(
                '.*/test/torque$',
                glob.glob(f"{tarname}/deps/v8/test/*", recursive=True)
            ),
            only_dirs=True
        )

        self.remove(
            anti_glob(
                '.*/test/torque/.*',
                glob.glob(f"{tarname}/deps/v8/test/**", recursive=True)
            ),
            only_files=True
        )

        self.remove(
            anti_glob(
                '.*/contrib/optimizations$',
                glob.glob(f"{tarname}/deps/zlib/contrib/**", recursive=True)
            ),
            only_dirs=True
        )

        self.remove(
            max_depth(
                3,
                glob.glob(f"{tarname}/deps/v8/test/**", recursive=True)
            ),
            only_files=True
        )

        for file in os.walk(tarname):
            if os.path.islink(file):
                os.remove(file)

        shutil.make_archive(tarname, "tar", tarname)
        self.remove(tarname)
        # convert the tar to a gzip w/ level 9 compression
        with open(f"{tarname}.tar", "rb") as f:
            with gzip.open(f"{tarname}.tar.gz", "wb", compresslevel=9) as f_out:
                shutil.copyfileobj(f, f_out)
                f_out.close()
            f.close()

        self.remove(f"{tarname}.tar")
        return 0

    def target_tar_upload(self, target, options):
        self.dependencies(['tar'], options=options)
        self.exec(['ssh', options.get('staging_server'), f'mkdir -p nodejs/{options.get("distribution_type")}/{options.get("full_version")}'], options=options)
        self.exec(['chmod', '664', f'{options.get("tar_name")}.tar.gz'], options)
        self.exec([
            'scp',
            f'{options.get("tar_name")}.tar.gz',
            f'{options.get("staging_server")}:nodejs/{options.get("distribution_type")}/{options.get("full_version")}/'],
            options
        )
        return self.exec([
            'ssh',
            options.get('staging_server'),
            f'touch nodejs/{options.get("distribution_type")}/{options.get("full_version")}/{options.get("tar_name")}.tar.gz.done'],
            options
        )

    def target_doc_upload(self, target, options):
        self.dependencies(['doc'], options=options)
        self.exec(['ssh', options.get('staging_server'), f'mkdir -p nodejs/{options.get("distribution_type")}/{options.get("full_version")}/docs/'], options=options)
        self.exec(['chmod', '-R', 'ug=rw-x=X,o=r+X', 'out/doc'], options)
        self.exec(['scp', '-pr', 'out/doc/*', f'{options.get("staging_server")}:nodejs/{options.get("distribution_type")}/{options.get("full_version")}/docs/'], options)
        return self.exec(['ssh', options.get('staging_server'), f'touch nodejs/{options.get("distribution_type")}/{options.get("full_version")}/docs.done'], options)

    def target_tarball_headers(self, target, options):
        self.dependencies(['release-only'], options=options)
        tarname = options.get('tar_name')
        self.exec([
            sys.executable,
            './configure.py',
            '--prefix=/',
            f'--dest-cpu={options.get("arch")}',
            f'--tag={options.get("tag")}',
            f'--release-urlbase={options.get("release_urlbase")}',
        ] + options.get('config_flags') + options.get('build_release_flags'), options=options)
        self.exec([
            sys.executable,
            os.path.join('tools', 'install.py'),
            'install', '--headers-only',
            '--dest-dir', tarname,
            '--prefix', '/',
        ])

        for file in os.walk(tarname):
            if os.path.islink(file):
                os.remove(file)

        shutil.make_archive(f'{tarname}-headers', "tar", tarname)
        self.remove(tarname)
        # convert the tar to a gzip w/ level 9 compression
        with open(f"{tarname}-headers.tar", "rb") as f:
            with gzip.open(f"{tarname}-headers.tar.gz", "wb", compresslevel=9) as f_out:
                shutil.copyfileobj(f, f_out)
                f_out.close()
            f.close()

        self.remove(f"{tarname}-headers.tar")
        return 0

    def target_tar_headers_upload(self, target, options):
        self.dependencies(['tarball-headers'], options=options)
        self.exec(['ssh', options.get('staging_server'), f'mkdir -p nodejs/{options.get("distribution_type")}/{options.get("full_version")}/'], options=options)
        self.exec(['chmod', '664', f'{options.get("tar_name")}-headers.tar.gz'], options)
        self.exec([
            'scp',
            f'{options.get("tar_name")}-headers.tar.gz',
            f'{options.get("staging_server")}:nodejs/{options.get("distribution_type")}/{options.get("full_version")}/'
        ], options)
        return self.exec([
            'ssh',
            options.get('staging_server'),
            f'touch nodejs/{options.get("distribution_type")}/{options.get("full_version")}/{options.get("tar_name")}-headers.tar.gz.done'
        ], options)

    def target_binary_tar(self, target, options):
        self.dependencies(['release-only'], options=options)

        binary_name = options.get('binary_name')
        self.remove([binary_name, 'out/deps', 'out/Release'])
        self.exec([
            sys.executable,
            './configure.py',
            f'--dest-cpu={options.get("arch")}',
            f'--tag={options.get("tag")}',
            f'--release-urlbase={options.get("release_urlbase")}',
        ] + options.get('config_flags') + options.get('build_release_flags'), options=options)

        self.make('install', allow_remake=True, must_succeed=False, options=options.new(environ={'DESTDIR': binary_name, 'PORTABLE': '1'}))
        shutil.copy('LICENSE', os.path.join(binary_name, 'LICENSE'))
        shutil.copy('README.md', os.path.join(binary_name, 'README.md'))

        if os.path.exists(options.get('changelog')):
            shutil.copy(options.get('changelog'), os.path.join(binary_name, 'CHANGELOG.md'))
        else:
            shutil.copy('CHANGELOG.md', os.path.join(binary_name, 'CHANGELOG.md'))

        if options.get('platform') == 'darwin':
            self.exec(['sh', 'tools/osx-codesign.sh'], options=options.new(environ={
                'SIGN': options.get('codesign_certificate'),
                'PKGDIR': binary_name,
            }))

        shutil.make_archive(binary_name, "tar", binary_name)
        self.remove(binary_name)

        with open(f"{binary_name}.tar", "rb") as f:
            with gzip.open(f"{binary_name}.tar.gz", "wb", compresslevel=9) as f_out:
                shutil.copyfileobj(f, f_out)
                f_out.close()
            f.close()

        self.remove(f"{binary_name}.tar")
        return 0

    def target_binary_upload(self, target, options):
        self.dependencies(['binary'], options=options)
        self.exec(['ssh', options.get('staging_server'), f'mkdir -p nodejs/{options.get("distribution_type")}/{options.get("full_version")}/'], options=options)
        self.exec(['chmod', '664', f'{options.get("binary_name")}.tar.gz'], options)
        self.exec([
            'scp',
            f'{options.get("binary_name")}.tar.gz',
            f'{options.get("staging_server")}:nodejs/{options.get("distribution_type")}/{options.get("full_version")}/'
        ], options)
        return self.exec([
            'ssh',
            options.get('staging_server'),
            f'touch nodejs/{options.get("distribution_type")}/{options.get("full_version")}/{options.get("binary_name")}.tar.gz.done'
        ], options)

    def target_bench_addons_build(self, target, options):
        self.dependencies([options.get('node_exe'), 'benchmark/napi/.buildstamp'], must_make=False, options=options)
        return 0

    def target_bench_addons_clean(self, target, options):
        self.remove(['benchmark/napi/.buildstamp'] + glob.glob('benchmark/napi/*/build'))
        return 0

    def target_lint_md_rollup(self, target, options):
        self.remove(glob.glob('tools/.*mdlintstamp'))
        os.chdir(os.path.join('tools', 'lint-md'))
        self.exec(['npm', 'ci'], options)
        self.exec(['npm', 'run', 'build'], options)
        os.chdir(os.path.join('..', '..'))
        return 0

    def target_lint_md_clean(self, target, options):
        self.remove(glob.glob('tools/.*mdlintstamp') + glob.glob('tools/lint-md/node_modules'))
        return 0

    def target_tools_mdlintstamp(self, target, options):
        self.try_node(os.path.join('tools', 'lint-md', 'lint-md.mjs'), *options.get('lint_markdown_files'))
        open(os.path.join('tools', '.mdlintstamp'), 'w').close()
        return 0

    def target_lint_md(self, target, options):
        self.dependencies(['tools/.mdlintstamp', 'lint-js-doc'], options=options)
        return 0

    def target_format_md(self, target, options):
        self.try_node(os.path.join('tools', 'lint-md', 'lint-md.mjs'), '--format', *options.get('lint_markdown_files'))
        return 0

    def target_lint_js(self, target, options):
        is_fix_mode = CLI.has('--fix')

        return self.exec([
            os.path.join('tools', 'node_modules', 'eslint', 'bin', 'eslint.js'),
            '--cache',
            '--max-warnings=0',
            '--report-unused-disable-directives',
        ] + options.get('lint_javascript_files') + (['--fix'] if is_fix_mode else []), options)

    def target_lint_js_ci(self, target, options):
        return self.exec([
            os.path.join('tools', 'node_modules', 'eslint', 'bin', 'eslint.js'),
            '--cache',
            '--max-warnings=0',
            '--report-unused-disable-directives',
            '-f', 'tap',
            '-o', 'test-eslint.tap',
        ] + options.get('lint_javascript_files'), options)

    def target_format_cpp_build(self, target, options):
        os.chdir('tools/clang-format')
        self.exec([
            os.path.join(os.getcwd(), options.get('npm')),
            'ci',
        ], options)
        os.chdir(os.path.join('..', '..'))
        return 0

    def target_format_cpp_clean(self, target, options):
        self.remove(glob.glob('tools/clang-format/node_modules'))
        return 0

    def target_format_cpp(self, target, options):
        if not os.path.exists(os.path.join(os.getcwd(), "tools", "clang-format", "node_modules")):
            self.logger.error("Please run 'format-cpp-build' first to install the necessary dependencies")
            sys.exit(1)

        return self.exec([
            sys.executable,
            os.path.join(os.getcwd(), "tools", "clang-format", "node_modules", ".bin", "git-clang-format"),
            "--binary=tools/clang-format/node_modules/.bin/clang-format",
            "--style=file",
            options.get('clang_format_start'), "--",
        ] + options.get('format_cpp_files'), options)

    def target_tools_cpplintstamp(self, target, options):
        self.dependencies(options.get('lint_cpp_files'), options=options)
        self.exec([sys.executable, os.path.join('tools', 'cpplint.py')] + options.get('lint_cpp_files'), options)
        self.exec([sys.executable, os.path.join('tools', 'checkimports.py')] + options.get('lint_cpp_files'), options)
        open(os.path.join('tools', '.cpplintstamp'), 'w').close()

    def target_tools_doclintstamp(self, target, options):
        self.dependencies(['test/addons/.docbuildstamp'], options=options)

        return self.exec([
            sys.executable,
            os.path.join(os.getcwd(), "tools", "cpplint.py"),
            f"--filter={','.join(options.get('addon_doc_lint_flags'))}",
            "test/addons/??_*/*.cc",
            "test/addons/??_*/*.h"
        ], options)

    def target_lint_py_build(self, target, options):
        return self.exec([
            sys.executable,
            "-m", "pip",
            "install", "--upgrade",
            "-t", "tools/pip/site-packages",
            "ruff==0.3.4"
        ], options)

    def target_lint_py(self, target, options):
        if not os.path.exists(os.path.join(os.getcwd(), "tools", "pip", "site-packages", "ruff")):
            self.logger.error("Please run 'lint-py-build' first to install the necessary dependencies")
            return 1

        self.exec([
            os.path.join(os.getcwd(), "tools", "pip", "site-packages", "bin", "ruff"),
            "--version"
        ], options)

        is_fix_mode = CLI.has('--fix')

        self.exec([
            os.path.join(os.getcwd(), "tools", "pip", "site-packages", "bin", "ruff"),
            "check",
            "."
        ] + (["--fix"] if is_fix_mode else []), options)

        return 0

    def target_lint_yaml_build(self, target, options):
        return self.exec([
            sys.executable,
            "-m", "pip",
            "install", "--upgrade",
            "-t", "tools/pip/site-packages",
            "yamllint"
        ], options)

    def target_lint_yaml(self, target, options):
        if not os.path.exists(os.path.join(os.getcwd(), "tools", "pip", "site-packages", "yamllint")):
            self.logger.error("Please run 'lint-yaml-build' first to install the necessary dependencies")
            return 1

        return self.exec([
            sys.executable,
            "-m", "yamllint",
            "."
        ], options=options.new(
            environ={
                'PYTHONPATH': os.path.join(os.getcwd(), "tools", "pip")
            }
        ))

    def target_lint(self, target, options):
        if not os.path.exists(os.path.join(os.getcwd(), "tools", "node_modules", "eslint")):
            self.logger.error("Linting is not available through the source tarball")
            self.logger.error("Use the git repo (https://github.com/nodejs/node) instead")
            return 1

        js_exit = self.make("lint-js", allow_remake=True, options=options)
        cpp_exit = self.make("lint-cpp", allow_remake=True, options=options)
        addon_docs_exit = self.make("lint-addon-docs", allow_remake=True, options=options)
        md_exit = self.make("lint-md", allow_remake=True, options=options)
        yaml_exit = self.make("lint-yaml", allow_remake=True, options=options)

        return js_exit + cpp_exit + addon_docs_exit + md_exit + yaml_exit

    def target_lint_ci(self, target, options):
        self.dependencies(['lint-js-ci', 'lint-cpp', 'lint-py', 'lint-md', 'lint-addon-docs', 'lint-yaml-build', 'lint-yaml'], options=options)
        if not os.path.exists(os.path.join(os.getcwd(), "tools", "node_modules", "eslint")):
            self.logger.error("Linting is not available through the source tarball")
            self.logger.error("Use the git repo (https://github.com/nodejs/node) instead")
            return 1

        conflicts = self.exec(["git", "diff", "--check"])
        conflicts = [conflict.split(":")[0] for conflict in conflicts.splitlines()]

        if conflicts:
            self.logger.error("Found conflicts in the following files:")
            for conflict in conflicts:
                self.logger.error(conflict)
            return 1

        return 0

    def target_lint_clean(self, target, options):
        self.remove(glob.glob('tools/.*lintstamp') + ['.eslintcache'])
        return 0

    def target_gen_openssl(self, target, options):
        self.exec(['docker', 'build', '-t', 'node-openssl-builder', 'deps/openssl/config'], options)
        return self.exec(['docker', 'run', '-it', '-v', f'{os.getcwd()}:/node', 'node-openssl-builder', 'make', '-C', 'deps/openssl/config'], options)

def main():
    option = CLI.unescape(CLI.get_target_arg() or 'all')
    if option is None:
        Logger.print("ERROR", 31, "Invalid target specified")
        sys.exit(1)
    builder = Builder()
    sys.exit(builder.make(option))

if __name__ == "__main__":
    main()
