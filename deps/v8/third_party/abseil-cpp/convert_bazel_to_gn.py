#!/usr/bin/env python3
"""Script to generate BUILD.gn files from Abseil's BUILD.bazel at roll time."""

import ast
import datetime
import logging
import os
import re
import subprocess
import sys

# List of targets that do not have matching gn target generated.
_SKIP_TARGETS = {
    'container:btree_test':
    'TODO(mbonadei): Fix issue with EXPECT_DEATH and uncomment.',
    'container:container_memory_test':
    'Disabled because container_memory_test requires -frtti',
    'container:raw_hash_set_test':
    'raw_hash_set_test uses typeid(), i.e., relies on RTTI.',
    'debugging:demangle_test': 'Disabled because this test relies on RTTI',
    'flags:commandlineflag_test':
    'Conflicts at link_time with "parse_test" because defines the same flags.',
    'flags:reflection_test':
    'Conflicts at link time with "flag_test" because defines the same flags.',
    'log:check_test': 'Overlaps with absl_check_test',
    'log:log_basic_test': 'Overlaps with absl_log_basic_test',
    'profiling:sample_recorder_test':
    'TODO: Re-enable once the issue with gmock activating generic gtest printers hitting issues with -Wmicrosoft-cast.',
    'random/internal:nanobenchmark': 'Only used for benchmarking',
    'synchronization:blocking_counter_test':
    'Conflicts at link time with "tracing_strong_test" because also defines strong functions for AbslInternalTraceWait and alike',
    'synchronization:mutex_method_pointer_test': 'Doesn\'t compile.',
    'synchronization:notification_test':
    'Conflicts at link time with "tracing_strong_test" because also defines strong functions for AbslInternalTraceWait and alike',
    'types:any_span_test':
    'any_span_test is not ported because relies on RTTI',
}

# Targets that are public in absl in general, but are not exposed to chromium targets
_PRIVATE_TARGETS = {
    # Flags rely on static initializers and thus are not exposed in chromium
    'flags:config',
    'flags:commandlineflag',
    'flags:flag',
    'flags:marshalling',
    'flags:parse',
    'flags:reflection',
    'flags:usage',
    'log:flags',
    # Targets below expose macros that conflicts with similar chromium macros,
    # there are targets with alternative absl macro prefixed with ABSL_.
    'log:log',
    'log:check',
    'log:vlog_is_on',
    # absl::any, same as std::any doesn't work with chromium component builds.
    'types:any',
    # currently public, but shouldn't be, there are TODOs to make them private
    'base:malloc_internal',
    'cleanup:cleanup_internal',
    'container:compressed_tuple',
    'container:raw_hash_set',
    'strings:internal',
    # public in cctz, private for abseil users.
    'time/internal/cctz:civil_time',
    'time/internal/cctz:time_zone',
}

# Dependencies that preferably shouldn't be public in chromium, but are.
_PUBLIC_TARGETS = {
    'base:dynamic_annotations',
    'base:raw_logging_internal',
    'container:layout',
}

# Extra output added just before the target.
_ADD_PREFIX = {
    'base:c_header_test':
    'if (absl_build_tests) { import("//testing/test.gni") ',
    'flags:config':
    '''# Since absl/flags are only used by some test binaries (e.g. in WebRTC),
# there is no need to strip flags from mobile platforms binaries.
# This does not affect Chromium.
config("absl_flags_config") {
  defines = [ "ABSL_FLAGS_STRIP_NAMES=0" ]
 }
 ''',
    'log:check':
    '''# This target is banned both for 1st party and 3rd party code, use absl_check
# in third_party code instead.
# This header introduces CHECK macros that collides with chromium's macros.
# Instead libraries should use ABSL_CHECK that provides the same functionality.''',
    'log:log':
    '''# This target is banned both for 1st party and 3rd party code. Use absl_log
# in third_party code instead.
# This header introduces LOG macros that collides with chromium's macros.
# Instead libraries should use ABSL_LOG that provides the same functionality.''',
    'log:vlog_is_on':
    '''# This target is banned both for 1st party and 3rd party code, use ABSL_
# prefixed macros and absl_vlog_is_on in third_party code instead.'''
}

# Extra build rules added at the end. The reason they are needed vary per target.
_ADD_CONTENT = {
    'base:c_header_test':
    '}',  # Closes extra '{' opened by prefix.
    'cleanup:cleanup_internal':
    'visibility = [ "//third_party/abseil-cpp/absl/*" ]',
    'container:hashtablez_sampler_test':
    'if (is_win) { sources = [] }',
    'container:test_allocator':
    'deps = [ "//third_party/abseil-cpp/absl/base:config", "//third_party/googletest:gtest" ]',
    'flags:config':
    'public_configs = [ ":absl_flags_config" ]',
    'flags:parse_test':
    'if (is_ios) { sources = [] }',
    'log:absl_check_test':
    'if (is_ios) { sources = [] }',
    'log:log_sink_test':
    'if (is_ios) { sources = [] }',
    'log:scoped_mock_log_test':
    'if (is_ios) { sources = [] }',
    'log/internal:log_sink_set':
    'if (is_android) { libs = [ "log" ]  }',
    'log/internal:stderr_log_sink_test':
    'if (is_apple || is_android) { sources = [] }',
    'random:uniform_real_distribution_test':
    'if (is_ios) { sources = [] }',
    'random/internal:seed_material':
    'if (is_win) {  libs = [ "bcrypt.lib" ]}',
    'strings:strings':
    '''public_deps = [
    # string_view.h was once part of :strings, so string_view.h is
    # re-exported for backwards compatibility.
    # New code should directly depend on :string_view.
    # TODO(crbug.com/40276308): Remove once all targets are migrated to
    # :string_view.
    ":string_view" ]''',
    'strings:str_format_convert_test':
    'if (is_fuchsia) { sources = [] }',
    # Instead of parsing 'select', add platform-specific rules manually while there are few of those.
    'time/internal/cctz:time_zone':
    '''
if (is_win) { sources += [ "src/time_zone_name_win.cc" ]
              public += [ "src/time_zone_name_win.h" ] }
defines = []
if (is_apple) {
  frameworks = [ "Foundation.framework" ]
  # Work-around for https://github.com/llvm/llvm-project/issues/117630
  defines += [ "_XOPEN_SOURCE=700" ]
}
if (is_fuchsia) {
  deps += [
    "//third_party/fuchsia-sdk/sdk/fidl/fuchsia.intl:fuchsia.intl_hlcpp",
    "//third_party/fuchsia-sdk/sdk/pkg/async",
    "//third_party/fuchsia-sdk/sdk/pkg/async-loop-cpp",
    "//third_party/fuchsia-sdk/sdk/pkg/sys_cpp",
    "//third_party/fuchsia-sdk/sdk/pkg/zx",
  ]
}''',
    'time/internal/cctz:time_zone_name_win_test':
    'if (is_win) { sources = [ "src/time_zone_name_win_test.cc" ] }',
}


def _ast_get_value(node, local_vars):
    if isinstance(node, ast.Constant):
        return node.value
    elif isinstance(node, ast.List):
        return [_ast_get_value(elt, local_vars) for elt in node.elts]
    elif isinstance(node, ast.Name):
        if node.id in local_vars:
            return local_vars[node.id]
        return node.id
    elif isinstance(node, ast.BinOp) and isinstance(node.op, ast.Add):
        left = _ast_get_value(node.left, local_vars) or []
        right = _ast_get_value(node.right, local_vars) or []
        return left + right
    return None


class _Converter:

    def __init__(self, path, old_gn_content=None):
        self.bazel_targets = []
        self.test_targets = []
        self.public_targets = []
        self.packages = {}
        self.year = str(datetime.datetime.now().year)
        self.path = path
        m = re.search(r'.*/absl/(.*)', path)
        self.rel_path = m.group(1) if m else ''
        if old_gn_content:
            m = re.search(r'# Copyright (\d{4})', old_gn_content)
            if m:
                self.year = m.group(1)

    def _translate_dep(self, dep):
        if dep.startswith('@do_not_use'):
            return None
        if dep == '//absl/' + self.rel_path:
            return ':' + self.rel_path.split('/')[-1]
        if dep.startswith(':'):
            return dep
        if dep.startswith('//absl/' + self.rel_path + ':'):
            return dep[7 + len(self.rel_path):]
        if dep.startswith('//absl'):
            return '//third_party/abseil-cpp/' + dep[2:]
        if dep.startswith('@googletest'):
            return None
        return dep

    def _translate_visibility(self, vis_list):
        result = []
        for vis in vis_list:
            if vis == '//visibility:public':
                return []
            if vis == '//visibility:private':
                result.append(':*')
            if vis == '//absl:friends':
                continue
            if vis.startswith('//absl'):
                parts = vis.split(':')
                pkg = parts[0]
                rule = parts[1] if len(parts) > 1 else ''
                gn_pkg = '' if pkg[7:] == self.rel_path else (
                    '//third_party/abseil-cpp/' + pkg[2:])
                if rule == '__pkg__':
                    result.append(gn_pkg + ':*')
                elif rule == '__subpackages__':
                    result.append(gn_pkg + '/*')
                else:
                    result.append(gn_pkg + ':' + rule)
            if vis.startswith(':') and vis[1:] in self.packages:
                result += self.packages[vis[1:]]
        # In bazel targets are implicitly visible to the same BUILD file
        # in gn such visibility should be explicit.
        if ':*' not in result and '//third_party/abseil-cpp/absl/*' not in result:
            result.append(':*')
        return result

    def _translate_package(self, package):
        if package.endswith('/...'):
            return '//third_party/abseil-cpp' + package[1:-4] + "/*"
        else:
            path = package[7:]
            gn_path = '' if path == self.rel_path else '//third_party/abseil-cpp/absl/' + path
            return gn_path + ':*'

    # Targets that include string_view.h should depend on string_view target.
    # For backward compatibility it is possible to depend just on 'strings', but it is
    # better to depend on string_view directly.
    def _need_to_add_string_view(self, bt):
        string_target = ':strings' if self.rel_path == 'strings' else '//absl/strings'
        if string_target not in bt.get('deps', []):
            # If target doesn't depend on 'strings', it either doesn't use string_view,
            # or already directly depends on string_view. Skip reading source files.
            return False

        for s in bt.get('srcs', []) + bt.get('hdrs', []):
            with open(os.path.join(self.path, s), 'r', encoding='utf-8') as f:
                if re.search(r'#include "absl/strings/string_view.h"',
                             f.read()):
                    return True
        return False

    def parse_bazel(self, content):
        tree = ast.parse(content)
        local_vars = {
            'ABSL_DEFAULT_LINKOPTS': [],
            'ABSL_DEFAULT_COPTS': [],
            'ABSL_TEST_COPTS': []
        }
        default_vis = []
        for node in tree.body:
            if isinstance(node, ast.Assign):
                for target in node.targets:
                    if isinstance(target, ast.Name):
                        local_vars[target.id] = _ast_get_value(
                            node.value, local_vars)
            if isinstance(node, ast.Expr) and isinstance(node.value, ast.Call):
                call = node.value
                if isinstance(call.func, ast.Name):
                    func_name = call.func.id
                    if func_name == 'package':
                        for kw in call.keywords:
                            if kw.arg == 'default_visibility':
                                default_vis = _ast_get_value(
                                    kw.value, local_vars)
                    elif func_name == 'package_group':
                        name = None
                        packages = []
                        for kw in call.keywords:
                            if kw.arg == 'name':
                                name = _ast_get_value(kw.value, local_vars)
                            elif kw.arg == 'packages':
                                for p in _ast_get_value(kw.value, local_vars):
                                    packages.append(self._translate_package(p))
                        if name:
                            self.packages[name] = packages
                    elif func_name in ('cc_library', 'cc_test'):
                        t = {'is_test': func_name == 'cc_test'}
                        for kw in call.keywords:
                            t[kw.arg] = _ast_get_value(kw.value, local_vars)
                        if 'visibility' not in t and default_vis:
                            t['visibility'] = default_vis
                        self.bazel_targets.append(t)

    def generate(self):
        out = []
        out.append(f'# Copyright {self.year} The Chromium Authors')
        out.append(
            '# Use of this source code is governed by a BSD-style license that'
            ' can be')
        out.append('# found in the LICENSE file.')
        out.append('#')
        out.append('# Generated file. DO NOT EDIT.')
        out.append('')
        out.append('import("//third_party/abseil-cpp/absl.gni")')
        out.append('')

        for bt in self.bazel_targets:
            name = bt.get('name')
            if not name:
                continue

            is_test = bt.get('is_test')
            rule = 'absl_test' if is_test else 'absl_source_set'
            bazel_deps = bt.get('deps', [])
            target_name = f"{self.rel_path}:{name}"

            skip = _SKIP_TARGETS.get(target_name)
            if skip:
                out.append(f'# {skip}')
                out.append(f'# {rule}("{name}")')
                out.append(f'')
                continue

            if '@google_benchmark//:benchmark_main' in bazel_deps:
                # Porting benchmarks targets is not implemented.
                continue

            if '//absl/base:exception_safety_testing' in bazel_deps:
                out.append(
                    f'# skipped because chromium doesn\'t use c++ exceptions')
                out.append(f'# {rule}("{name}")')
                out.append(f'')
                continue

            if is_test and target_name != 'base:c_header_test' and '@googletest//:gtest_main' not in bazel_deps:
                out.append(
                    f'# {name} is excluded because defines its own main function'
                )
                out.append(f'# {rule}("{name}")')
                out.append(f'')
                continue

            # Start writing the output.
            out.append(_ADD_PREFIX.get(target_name, ''))
            if target_name == 'base:c_header_test':
                # This test is ported despite having own main function.
                out.append('test("absl_c_header_test") {')
            else:
                out.append(f'{rule}("{name}") {{')

            if bt.get('testonly'):
                out.append('testonly = true')

            srcs = bt.get('srcs', [])
            if srcs:
                out.append('sources = [')
                for s in sorted(srcs):
                    out.append(f'"{s}",')
                out.append(']')

            hdrs = bt.get('hdrs', []) + bt.get('textual_hdrs', [])
            if hdrs:
                out.append('public = [')
                for h in sorted(hdrs):
                    # One exception where header is excluded
                    if target_name == 'strings:strings' and h == 'string_view.h':
                        continue
                    out.append(f'"{h}",')
                out.append(']')

            vis = [] if is_test else self._translate_visibility(
                bt.get('visibility', []))
            if target_name in _PUBLIC_TARGETS:
                if not vis:
                    # Turn generally public target into public to abseil only.
                    vis.append("//third_party/abseil-cpp/absl/*")
                vis.append('//third_party/abseil-cpp:absl_component_deps')
            # empty visibility is handled by the rule template, in particular for non-tests
            # it imply public visibility, but in component builds it is still restricted.
            if vis:
                out.append('visibility = [')
                for v in sorted(vis):
                    out.append(f'"{v}",')
                out.append(']')

            gn_deps = []
            for d in bazel_deps:
                td = self._translate_dep(d)
                if td:
                    gn_deps.append(td)
                elif not is_test and d == '@googletest//:gtest':
                    gn_deps.append("//third_party/googletest:gmock")
                    gn_deps.append("//third_party/googletest:gtest")
            if self._need_to_add_string_view(bt):
                gn_deps.append(
                    self._translate_dep('//absl/strings:string_view'))
            if gn_deps:
                out.append('deps = [')
                for d in sorted(gn_deps):
                    out.append(f'"{d}",')
                out.append(']')

            out.append(_ADD_CONTENT.get(target_name, ''))
            out.append('}')
            out.append('')

            if target_name == 'base:c_header_test':
                pass
            elif is_test:
                self.test_targets.append(target_name)
            elif not (bt.get('testonly') or target_name in _PRIVATE_TARGETS
                      or vis):
                self.public_targets.append(target_name)

        return '\n'.join(out)


def convert_one(path):
    bazel_path = os.path.join(path, 'BUILD.bazel')
    gn_path = os.path.join(path, 'BUILD.gn')
    logging.info(f'Converting {bazel_path}')
    old_gn = None
    if os.path.exists(gn_path):
        with open(gn_path, 'r', encoding='utf-8') as f:
            old_gn = f.read()

    with open(bazel_path, 'r', encoding='utf-8') as f:
        bazel_content = f.read()

    converter = _Converter(path, old_gn)
    converter.parse_bazel(bazel_content)

    if not converter.bazel_targets:
        logging.info(f'Skipping {bazel_path} (no cc_library/cc_test targets)')
        return [], []

    new_gn = converter.generate()

    with open(gn_path, 'w', encoding='utf-8', newline='') as f:
        subprocess.run(['gn', 'format', '--stdin'],
                       check=True,
                       input=new_gn,
                       stdout=f,
                       stderr=subprocess.DEVNULL,
                       text=True)

    return converter.test_targets, converter.public_targets


def convert_all(root_dir):
    all_test_targets = []
    all_public_targets = []
    for dirpath, dirnames, filenames in os.walk(os.path.join(root_dir,
                                                             'absl')):
        if 'BUILD.bazel' in filenames:
            t, p = convert_one(dirpath)
            all_test_targets.extend(t)
            all_public_targets.extend(p)

    # Update root BUILD.gn
    root_gn_path = os.path.join(root_dir, 'BUILD.gn')
    if not os.path.exists(root_gn_path):
        logging.error(f"Failed to find {root_gn_path}")
        return

    logging.info(f"Updating root targets in {root_gn_path}")
    with open(root_gn_path, 'r') as f:
        content = f.read()

    # Update absl_component_deps
    pattern_comp = re.compile(
        r'(group\("absl_component_deps"\)\s*\{\s*public_deps\s*=\s*)\[([\s\S]*?)\]'
    )
    libs_lines = [
        f'"//third_party/abseil-cpp/absl/{label}"'
        for label in sorted(all_public_targets)
    ]
    new_libs_str = "[\n" + ",\n".join(libs_lines) + "\n]"
    content = pattern_comp.sub(r'\1' + new_libs_str, content, count=1)

    # Update absl_tests
    pattern_test = re.compile(
        r'(test\("absl_tests"\)\s*\{[\s\S]*?deps\s*=\s*)\[([\s\S]*?)\]')
    deps_lines = [f'"absl/{label}"' for label in sorted(all_test_targets)]
    deps_lines.append('"//third_party/googletest:gtest_main"')
    new_deps_str = "[" + ",\n".join(deps_lines) + "]"
    content = pattern_test.sub(r'\1' + new_deps_str, content, count=1)

    with open(root_gn_path, 'w', encoding='utf-8', newline='') as f:
        subprocess.run(['gn', 'format', '--stdin'],
                       check=True,
                       input=content,
                       stdout=f,
                       stderr=subprocess.DEVNULL,
                       text=True)


if __name__ == '__main__':
    logging.getLogger().setLevel(logging.INFO)

    if len(sys.argv) == 2 and os.path.isdir(sys.argv[1]):
        convert_one(sys.argv[1])
    elif len(sys.argv) < 2:
        if not os.getcwd().endswith('src') or not os.path.exists(
                'chrome/browser'):
            logging.error('Run this script from a chromium/src/ directory.')
            exit(1)
        convert_all(os.path.join(os.getcwd(), 'third_party', 'abseil-cpp'))
    else:
        logging.info('Usage: convert_bazel_to_gn.py [<dir>]')
        exit(1)
