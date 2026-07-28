#!/usr/bin/env python3
"""Script to generate BUILD.gn files from Abseil's BUILD.bazel at roll time."""

# This script is a work in progress and doesn't handle all corner cases of
# bazel->gn translation in abseil. It can convert some simple build files,
# support for for other build files planned to be added gradually.
# Review result of this script as if BUILD changes were made manually!

import ast
import datetime
import logging
import os
import re
import subprocess
import sys

# List of targets that do not have matching gn target generated.
_SKIP_TARGETS = {
    'types:any_span_test':
    'any_span_test is not ported because relies on RTTI',
}

def _ast_get_value(node):
    if isinstance(node, ast.Constant):
        return node.value
    elif isinstance(node, ast.List):
        return [_ast_get_value(elt) for elt in node.elts]
    elif isinstance(node, ast.Name):
        return node.id
    return None


class _Converter:

    def __init__(self, path, old_gn_content=None):
        self.bazel_targets = []
        self.year = str(datetime.datetime.now().year)
        self.path = path
        m = re.search(r'.*/absl/(.*)', path)
        self.rel_path = m.group(1) if m else ''
        if old_gn_content:
            m = re.search(r'# Copyright (\d{4})', old_gn_content)
            if m:
                self.year = m.group(1)

    def _translate_dep(self, dep):
        if dep.startswith(':'):
            return dep
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
                gn_pkg = '//third_party/abseil-cpp/' + pkg[2:]
                if rule == '__pkg__':
                    result.append(gn_pkg + ':*')
                elif rule == '__subpackages__':
                    result.append(gn_pkg + '/*')
                else:
                    result.append(gn_pkg + ':' + rule)
        # In bazel targets are implicitly visible to the same BUILD file
        # in gn such visibility should be explicit.
        if ':*' not in result and '//third_party/abseil-cpp/absl/*' not in result:
            result.append(':*')
        return result

    # Targets that include string_view.h should depend on string_view target.
    # For backward compatibility it is possible to depend just on 'strings', but it is
    # better to depend on string_view directly.
    def _need_to_add_string_view(self, bt):
        if "//absl/strings" not in bt.get('deps', []):
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
        default_vis = []
        for node in tree.body:
            if isinstance(node, ast.Expr) and isinstance(node.value, ast.Call):
                call = node.value
                if isinstance(call.func, ast.Name):
                    func_name = call.func.id
                    if func_name == 'package':
                        for kw in call.keywords:
                            if kw.arg == 'default_visibility':
                                default_vis = _ast_get_value(kw.value)
                    elif func_name in ('cc_library', 'cc_test'):
                        t = {'is_test': func_name == 'cc_test'}
                        for kw in call.keywords:
                            t[kw.arg] = _ast_get_value(kw.value)
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
            target_name = f"{self.rel_path}:{name}"

            skip = _SKIP_TARGETS.get(target_name)
            if skip:
                out.append(f'# {skip}')
                out.append(f'# {rule}("{name}")')
                out.append(f'')
                continue

            # Start writing the output.
            out.append(f'{rule}("{name}") {{')

            if bt.get('testonly'):
                out.append('testonly = true')

            srcs = bt.get('srcs', [])
            if srcs:
                out.append('sources = [')
                for s in sorted(srcs):
                    out.append(f'"{s}",')
                out.append(']')

            hdrs = bt.get('hdrs')
            if hdrs:
                out.append('public = [')
                for h in sorted(hdrs):
                    out.append(f'"{h}",')
                out.append(']')

            vis = ([] if is_test else self._translate_visibility(
                bt.get('visibility', [])))
            # empty visibility is handled by the rule template, in particular for non-tests
            # it imply public visibility, but in component builds it is still restricted.
            if vis:
                out.append('visibility = [')
                for v in sorted(vis):
                    out.append(f'"{v}",')
                out.append(']')

            gn_deps = []
            for d in bt.get('deps', []):
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

            out.append('}')
            out.append('')

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
        return

    new_gn = converter.generate()

    with open(gn_path, 'w', encoding='utf-8', newline='') as f:
        subprocess.run(['gn', 'format', '--stdin'],
                       check=True,
                       input=new_gn,
                       stdout=f,
                       stderr=subprocess.DEVNULL,
                       text=True)


def convert_all(root_dir):
    # TODO: crbug.com/524565513: walk the root dir when script is fully ready to handle all edge cases.
    for folder in [
            'algorithm',
            'crc',
            'functional',
            'memory',
            'meta',
            'numeric',
            'status',
            'time',
            'types',
            'utility',
    ]:
        convert_one(os.path.join(root_dir, 'absl', folder))


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
