#!/usr/bin/env python3
#
# Copyright 2020 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys
import lark
import argparse
from contextlib import suppress
from collections import namedtuple
from datetime import datetime

# GN grammar from https://gn.googlesource.com/gn/+/master/src/gn/parser.cc.
GN_GRAMMAR = """
    ?file : statement_list

    ?statement     : assignment | call | condition
    ?lvalue        : IDENTIFIER | array_access | scope_access
    assignment     : lvalue assign_op expr
    call           : IDENTIFIER "(" [ expr_list ] ")" [ block ]
    condition      : "if" "(" expr ")" block [ "else" ( condition | block ) ]
    ?block         : "{" statement_list "}"
    statement_list : statement*

    array_access   : IDENTIFIER "[" expr "]"
    scope_access   : IDENTIFIER "." IDENTIFIER
    ?primary_expr  : IDENTIFIER | INTEGER | STRING | call
                   | array_access | scope_access | block
                   | "(" expr ")" -> par_expr
                   | array
    array          : "[" [ expr ( "," expr? )* ] "]"
    expr_list      : expr ( "," expr )*

    ?assign_op : "="  -> asgn_op
               | "+=" -> asgn_add_op
               | "-=" -> asgn_sub_op

    ?expr      : expr1
    ?expr1     : expr1 "||" expr2 -> or
               | expr2
    ?expr2     : expr2 "&&" expr3 -> and
               | expr3
    ?expr3     : expr3 "==" expr4 -> eq
               | expr3 "!=" expr4 -> ne
               | expr4
    ?expr4     : expr4 "<=" expr5 -> le
               | expr4 "<" expr5  -> lt
               | expr4 ">=" expr5 -> ge
               | expr4 ">" expr5  -> gt
               | expr5
    ?expr5     : expr5 "+" expr6  -> add
               | expr5 "-" expr6  -> sub
               | expr6
    ?expr6     : "!" primary_expr -> neg
               | primary_expr

    COMMENT : /#.*/

    %import common.ESCAPED_STRING -> STRING
    %import common.SIGNED_INT -> INTEGER
    %import common.CNAME -> IDENTIFIER
    %import common.WS
    %ignore WS
    %ignore COMMENT
"""

V8_TARGET_TYPES = (
    'v8_component',
    'v8_source_set',
    'v8_executable',
)

OPS = (
    'neg',
    'eq',
    'ne',
    'le',
    'lt',
    'ge',
    'gt',
    'and',
    'or',
)


class UnsupportedOperation(Exception):
    pass


class V8GNTransformer(object):
    """
    Traverse GN parse-tree and build resulting object.
    """
    def __init__(self, builder, filtered_targets):
        self.builder = builder
        self.filtered_targets = filtered_targets
        self.current_target = None

    def Traverse(self, tree):
        self.builder.BuildPrologue()
        self.TraverseTargets(tree)
        self.builder.BuildEpilogue()

    def TraverseTargets(self, tree):
        'Traverse top level GN targets and call the builder functions'
        for stmt in tree.children:
            if stmt.data != 'call':
                continue
            target_type = stmt.children[0]
            if target_type not in V8_TARGET_TYPES:
                continue
            target = stmt.children[1].children[0].strip('\"')
            if target not in self.filtered_targets:
                continue
            self.current_target = target
            self._Target(target_type, target, stmt.children[2].children)

    def _Target(self, target_type, target, stmts):
        stmts = self._StatementList(stmts)
        return self.builder.BuildTarget(target_type, target, stmts)

    def _StatementList(self, stmts):
        built_stmts = []
        for stmt in stmts:
            built_stmts.append(self._Statement(stmt))
        return [stmt for stmt in built_stmts if stmt]

    def _Statement(self, stmt):
        # Handle only interesting gn statements.
        with suppress(KeyError):
            return self.STATEMENTS[stmt.data](self, *stmt.children)

    def _Assignment(self, left, op, right):
        return self.ASSIGN_TYPES[op.data](self, left, right)

    def _AssignEq(self, left, right):
        if left == 'sources':
            return self.builder.BuildSourcesList(
                self.current_target, [str(token) for token in right.children])

    def _AssignAdd(self, left, right):
        if left == 'sources':
            return self.builder.BuildAppendSources(
                self.current_target, [str(token) for token in right.children])

    def _AssignSub(self, left, right):
        if left == 'sources':
            return self.builder.BuildRemoveSources(
                self.current_target, [str(token) for token in right.children])

    def _Condition(self, cond_expr, then_stmts, else_stmts=None):
        'Visit GN condition: if (cond) {then_stmts} else {else_stmts}'
        cond_expr = self._Expr(cond_expr)
        then_stmts = self._StatementList(then_stmts.children)
        if not then_stmts:
            # Ignore conditions with empty then stmts.
            return
        if else_stmts is None:
            return self.builder.BuildCondition(cond_expr, then_stmts)
        elif else_stmts.data == 'condition':
            else_cond = self._Condition(*else_stmts.children)
            return self.builder.BuildConditionWithElseCond(
                cond_expr, then_stmts, else_cond)
        else:
            assert 'statement_list' == else_stmts.data
            else_stmts = self._StatementList(else_stmts.children)
            return self.builder.BuildConditionWithElseStmts(
                cond_expr, then_stmts, else_stmts)

    def _Expr(self, expr):
        'Post-order traverse expression trees'
        if isinstance(expr, lark.Token):
            if expr.type == 'IDENTIFIER':
                return self.builder.BuildIdentifier(str(expr))
            elif expr.type == 'INTEGER':
                return self.builder.BuildInteger(str(expr))
            else:
                return self.builder.BuildString(str(expr))
        if expr.data == 'par_expr':
            return self.builder.BuildParenthesizedOperation(
                self._Expr(*expr.children))
        if expr.data not in OPS:
            raise UnsupportedOperation(
                f'The operator "{expr.data}" is not supported')
        if len(expr.children) == 1:
            return self._UnaryExpr(expr.data, *expr.children)
        if len(expr.children) == 2:
            return self._BinaryExpr(expr.data, *expr.children)
        raise UnsupportedOperation(f'Unsupported arity {len(expr.children)}')

    def _UnaryExpr(self, op, right):
        right = self._Expr(right)
        return self.builder.BuildUnaryOperation(op, right)

    def _BinaryExpr(self, op, left, right):
        left = self._Expr(left)
        right = self._Expr(right)
        return self.builder.BuildBinaryOperation(left, op, right)

    STATEMENTS = {
        'assignment': _Assignment,
        'condition': _Condition,
    }

    ASSIGN_TYPES = {
        'asgn_op': _AssignEq,
        'asgn_add_op': _AssignAdd,
        'asgn_sub_op': _AssignSub,
    }


TARGETS = {
    'v8_libbase': 'lib',
    'v8_cppgc_shared': 'lib',
    'cppgc_base': 'lib',
    'cppgc_standalone': 'sample',
    'cppgc_unittests_sources': 'tests',
    'cppgc_unittests': 'tests',
}


class CMakeBuilder(object):
    """
    Builder that produces the main CMakeLists.txt.
    """
    def __init__(self):
        self.result = []
        self.source_sets = {}

    def BuildPrologue(self):
        self.result.append(f"""
# Copyright {datetime.now().year} the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# This file is automatically generated by {__file__}. Do NOT edit it.

cmake_minimum_required(VERSION 3.11)
project(cppgc CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(CPPGC_ENABLE_OBJECT_NAMES "Enable object names in cppgc for debug purposes" OFF)
option(CPPGC_ENABLE_CAGED_HEAP "Enable heap reservation of size 4GB, only possible for 64bit archs" OFF)
option(CPPGC_ENABLE_VERIFY_HEAP "Enables additional heap verification phases and checks" OFF)
option(CPPGC_ENABLE_YOUNG_GENERATION "Enable young generation in cppgc" OFF)
set(CPPGC_TARGET_ARCH "x64" CACHE STRING "Target architecture, possible options: x64, x86, arm, arm64, ppc64, s390x, mips64el")

set(IS_POSIX ${{UNIX}})
set(IS_MAC ${{APPLE}})
set(IS_WIN ${{WIN32}})
if("${{CMAKE_SYSTEM_NAME}}" STREQUAL "Linux")
  set(IS_LINUX 1)
elseif("${{CMAKE_SYSTEM_NAME}}" STREQUAL "Fuchsia")
  set(IS_FUCHSIA 1)
endif()

set(CURRENT_CPU ${{CPPGC_TARGET_ARCH}})

if("${{CPPGC_TARGET_ARCH}}" STREQUAL "x64" OR
   "${{CPPGC_TARGET_ARCH}}" STREQUAL "arm64" OR
   "${{CPPGC_TARGET_ARCH}}" STREQUAL "ppc64" OR
   "${{CPPGC_TARGET_ARCH}}" STREQUAL "mips64el")
  if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "64-bit arch specified for 32-bit compiler")
  endif()
  set(CPPGC_64_BITS ON)
endif()

if(CPPGC_ENABLE_CAGED_HEAP AND NOT CPPGC_64_BITS)
  message(FATAL_ERROR "Caged heap is only supported for 64bit archs")
endif()

if(CPPGC_64_BITS)
  # Always enable caged heap for 64bits archs.
  set(CPPGC_ENABLE_CAGED_HEAP ON CACHE BOOL "Enable caged heap for 64bit" FORCE)
endif()

if(CPPGC_ENABLE_YOUNG_GENERATION AND NOT CPPGC_ENABLE_CAGED_HEAP)
  message(FATAL_ERROR "Young generation is only supported for caged heap configuration")
endif()

if(NOT CPPGC_64_BITS)
  if(NOT MSVC)
    set(CMAKE_CXX_FLAGS "${{CMAKE_CXX_FLAGS}} -m32")
    set(CMAKE_C_FLAGS "${{CMAKE_C_FLAGS}} -m32")
    set(CMAKE_EXE_LINKER_FLAGS "${{CMAKE_EXE_LINKER_FLAGS}} -m32")
    set(CMAKE_SHARED_LINKER_FLAGS "${{CMAKE_SHARED_LINKER_FLAGS}} -m32")
    set(CMAKE_MODULE_LINKER_FLAGS "${{CMAKE_MODULE_LINKER_FLAGS}} -m32")
  endif()
endif()

find_package(Threads REQUIRED)

include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY "https://chromium.googlesource.com/external/github.com/google/googletest.git"
  GIT_TAG        "4fe018038f87675c083d0cfb6a6b57c274fb1753"
  SOURCE_DIR     "${{CMAKE_BINARY_DIR}}/third_party/googletest/src"
)

FetchContent_GetProperties(googletest)
if(NOT googletest_POPULATED)
  FetchContent_Populate(googletest)
  message("Fetched googletest into ${{googletest_SOURCE_DIR}}")
  add_subdirectory(${{googletest_SOURCE_DIR}} ${{googletest_BINARY_DIR}} EXCLUDE_FROM_ALL)
  include_directories("${{CMAKE_BINARY_DIR}}")
endif()
""")

    def BuildEpilogue(self):
        self.result.extend(
            self._GenTargetString(target, sets)
            for target, sets in self.source_sets.items())
        self.result.append("\ninstall(TARGETS cppgc)")

    def BuildTarget(self, target_type, target, rules):
        # Don't generate CMake targets yet, defer it to build_epilogue.
        comment = f"""
#===============================================================================
# {self._CMakeTarget(target)} sources.
#==============================================================================="""
        self.result.append(comment)
        self.result.extend(rules)
        self.source_sets.setdefault(
            TARGETS[target], []).append('${' + self._SourceVar(target) + '}')

    def BuildSourcesList(self, target, sources):
        sources = self._ExpandSources(target, sources)
        return f'set({self._SourceVar(target)} {sources})'

    def BuildAppendSources(self, target, sources):
        sources = self._ExpandSources(target, sources)
        return f'list(APPEND {self._SourceVar(target)} {sources})'

    def BuildRemoveSources(self, target, sources):
        sources = self._ExpandSources(target, sources)
        return f'list(REMOVE_ITEM {self._SourceVar(target)} {sources})'

    def BuildCondition(self, cond, then_stmts):
        return f"""
if({cond})
  {' '.join(then_stmts)}
endif()
        """.strip()

    def BuildConditionWithElseStmts(self, cond, then_stmts, else_stmts):
        return f"""
if({cond})
  {' '.join(then_stmts)}
{'else() ' + ' '.join(else_stmts)}
endif()
        """.strip()

    def BuildConditionWithElseCond(self, cond, then_stmts, else_cond):
        return f"""
if({cond})
  {' '.join(then_stmts)}
else{else_cond}
        """.strip()

    def BuildParenthesizedOperation(self, operation):
        return ''.join(['(', operation, ')'])

    def BuildUnaryOperation(self, op, right):
        OPS = {
            'neg': 'NOT',
        }
        return ' '.join([OPS[op], right])

    def BuildBinaryOperation(self, left, op, right):
        if op == 'ne':
            neg_result = self.BuildBinaryOperation(left, 'eq', right)
            return self.BuildUnaryOperation('neg', neg_result)
        OPS = {
            'eq': 'STREQUAL',
            'le': 'LESS_EQUAL',
            'lt': 'LESS',
            'ge': 'GREATER_EQUAL',
            'gt': 'GREATER',
            'and': 'AND',
            'or': 'OR',
        }
        return ' '.join([left, OPS[op], right])

    def BuildIdentifier(self, token):
        return self._CMakeVarRef(token)

    def BuildInteger(self, integer):
        return integer

    def BuildString(self, string):
        return string

    def GetResult(self):
        return '\n'.join(self.result)

    @staticmethod
    def _GenTargetString(target_type, source_sets):
        Target = namedtuple('Target', 'name cmake deps desc')
        CMAKE_TARGETS = {
            'lib':
            Target(name='cppgc',
                   cmake='add_library',
                   deps=['Threads::Threads'],
                   desc='Main library'),
            'sample':
            Target(name='cppgc_hello_world',
                   cmake='add_executable',
                   deps=['cppgc'],
                   desc='Example'),
            'tests':
            Target(name='cppgc_unittests',
                   cmake='add_executable',
                   deps=['cppgc', 'gtest', 'gmock'],
                   desc='Unittests')
        }
        target = CMAKE_TARGETS[target_type]
        return f"""
# {target.desc} target.
{target.cmake}({target.name} {' '.join(source_sets)})

{'target_link_libraries(' + target.name + ' ' + ' '.join(target.deps) + ')' if target.deps else ''}

target_include_directories({target.name} PRIVATE "${{CMAKE_SOURCE_DIR}}"
                                         PRIVATE "${{CMAKE_SOURCE_DIR}}/include")

if(CPPGC_ENABLE_OBJECT_NAMES)
  target_compile_definitions({target.name} PRIVATE "-DCPPGC_SUPPORTS_OBJECT_NAMES")
endif()
if(CPPGC_ENABLE_CAGED_HEAP)
  target_compile_definitions({target.name} PRIVATE "-DCPPGC_CAGED_HEAP")
endif()
if(CPPGC_ENABLE_VERIFY_HEAP)
  target_compile_definitions({target.name} PRIVATE "-DCPPGC_ENABLE_VERIFY_HEAP")
endif()
if(CPPGC_ENABLE_YOUNG_GENERATION)
  target_compile_definitions({target.name} PRIVATE "-DCPPGC_YOUNG_GENERATION")
endif()"""

    @staticmethod
    def _ExpandSources(target, sources):
        if TARGETS[target] == 'tests':
            sources = ['\"test/unittests/' + s[1:] for s in sources]
        return ' '.join(sources)

    @staticmethod
    def _SourceVar(target):
        return CMakeBuilder._CMakeVar(target) + '_SOURCES'

    @staticmethod
    def _CMakeVar(var):
        return var.replace('v8_', '').upper()

    @staticmethod
    def _CMakeTarget(var):
        return var.replace('v8_', '')

    @staticmethod
    def _CMakeVarRef(var):
        return '\"${' + CMakeBuilder._CMakeVar(var) + '}"'


def FormatCMake(contents):
    from cmake_format import configuration, lexer, parse, formatter
    cfg = configuration.Configuration()
    tokens = lexer.tokenize(contents)
    parse_tree = parse.parse(tokens)
    box_tree = formatter.layout_tree(parse_tree, cfg)
    return formatter.write_tree(box_tree, cfg, contents)


def SaveContents(contents, outfile):
    if outfile == '-':
        return print(contents)
    with open(outfile, 'w+') as ofile:
        ofile.write(contents)


def ParseGN(contents):
    parser = lark.Lark(GN_GRAMMAR, parser='lalr', start='file')
    return parser.parse(contents)


def ParseGNFile(filename):
    with open(filename, 'r') as file:
        contents = file.read()
        return ParseGN(contents)


def GenCMake(main_gn, test_gn, outfile):
    tree = ParseGNFile(main_gn)
    tree.children.extend(ParseGNFile(test_gn).children)
    builder = CMakeBuilder()
    V8GNTransformer(builder, TARGETS.keys()).Traverse(tree)
    result = FormatCMake(builder.GetResult())
    SaveContents(result, outfile)


def Main():
    arg_parser = argparse.ArgumentParser(
        description=
        'Generate CMake from the main GN file for targets needed to build CppGC.'
    )
    arg_parser.add_argument('--out', help='output CMake filename', default='-')
    arg_parser.add_argument('--main-gn',
                            help='main BUILD.gn input file',
                            default='BUILD.gn')
    arg_parser.add_argument('--test-gn',
                            help='unittest BUILD.gn input file',
                            default='test/unittests/BUILD.gn')
    args = arg_parser.parse_args()

    GenCMake(args.main_gn, args.test_gn, args.out)
    return 0


if __name__ == '__main__':
    sys.exit(Main())
