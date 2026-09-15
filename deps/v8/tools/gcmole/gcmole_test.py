#!/usr/bin/env python3
# Copyright 2022 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from pathlib import Path

import collections
import os
import re
import shutil
import subprocess
import sys
import tempfile
import textwrap
import unittest

import gcmole

GCMOLE_PATH = Path(__file__).parent.absolute()
TESTDATA_PATH = GCMOLE_PATH / 'testdata' / 'v8'

Options = collections.namedtuple(
    'Options', ['v8_root_dir', 'v8_target_cpu', 'shard_count', 'shard_index',
                'test_run'])


def abs_test_file(f):
  return TESTDATA_PATH / f


class FilesTest(unittest.TestCase):

  def testFileList_for_testing(self):
    options = Options(TESTDATA_PATH, 'x64', 1, 0, True)
    self.assertEqual(
        gcmole.build_file_list(options),
        list(map(abs_test_file, ['tools/gcmole/gcmole-test.cc'])))

  def testFileList_x64(self):
    options = Options(TESTDATA_PATH, 'x64', 1, 0, False)
    expected = [
        'file1.cc',
        'file2.cc',
        'x64/file1.cc',
        'x64/file2.cc',
        'file3.cc',
        'file4.cc',
        'test/cctest/test-x64-file1.cc',
        'test/cctest/test-x64-file2.cc',
    ]
    self.assertEqual(
        gcmole.build_file_list(options),
        list(map(abs_test_file, expected)))

  def testFileList_x64_shard0(self):
    options = Options(TESTDATA_PATH, 'x64', 2, 0, False)
    expected = [
        'file1.cc',
        'x64/file1.cc',
        'file3.cc',
        'test/cctest/test-x64-file1.cc',
    ]
    self.assertEqual(
        gcmole.build_file_list(options),
        list(map(abs_test_file, expected)))

  def testFileList_x64_shard1(self):
    options = Options(TESTDATA_PATH, 'x64', 2, 1, False)
    expected = [
        'file2.cc',
        'x64/file2.cc',
        'file4.cc',
        'test/cctest/test-x64-file2.cc',
    ]
    self.assertEqual(
        gcmole.build_file_list(options),
        list(map(abs_test_file, expected)))

  def testFileList_arm(self):
    options = Options(TESTDATA_PATH, 'arm', 1, 0, False)
    expected = [
        'file1.cc',
        'file2.cc',
        'file3.cc',
        'file4.cc',
        'arm/file1.cc',
        'arm/file2.cc',
    ]
    self.assertEqual(
        gcmole.build_file_list(options),
        list(map(abs_test_file, expected)))


GC = 'Foo,NowCollectAllTheGarbage'
SP = 'Bar,SafepointSlowPath'
WF = 'Baz,WriteField'


class OutputLines:
  CALLERS_RE = re.compile(r'([\w,]+)\s*→\s*(.*)')

  def __init__(self, *callee_list):
    """Construct a test data placeholder for output lines of one invocation of
    the GCMole plugin.

    Args:
        callee_list: Strings, each containing a caller/calle relationship
            formatted as "A → B C", meaning A calls B and C. For GC,
            Safepoint and a allow-listed function use GC, SP and WF
            constants above respectivly.
            Methods not calling anything are formatted as "A →".
    """
    self.callee_list = callee_list

  def lines(self):
    result = []
    for str_rep in self.callee_list:
      match = self.CALLERS_RE.match(str_rep)
      assert match
      result.append(match.group(1))
      for callee in (match.group(2) or '').split():
        result.append('\t' + callee)
    return result


class SuspectCollectorTest(unittest.TestCase):

  def create_callgraph(self, *outputs):
    call_graph = gcmole.CallGraph()
    for output in outputs:
      call_graph.parse(output.lines())
    return call_graph

  def testCallGraph(self):
    call_graph = self.create_callgraph(OutputLines())
    self.assertDictEqual(call_graph.funcs, {})

    call_graph = self.create_callgraph(OutputLines('A →'))
    self.assertDictEqual(call_graph.funcs, {'A': set()})

    call_graph = self.create_callgraph(OutputLines('A → B'))
    self.assertDictEqual(call_graph.funcs, {'A': set(), 'B': set('A')})

    call_graph = self.create_callgraph(
        OutputLines('A → B C', 'B → C D', 'D →'))
    self.assertDictEqual(
        call_graph.funcs,
        {'A': set(), 'B': set('A'), 'C': set(['A', 'B']), 'D': set('B')})

    call_graph = self.create_callgraph(
        OutputLines('B → C D', 'D →'), OutputLines('A → B C'))
    self.assertDictEqual(
        call_graph.funcs,
        {'A': set(), 'B': set('A'), 'C': set(['A', 'B']), 'D': set('B')})

  def testCallGraphMerge(self):
    """Test serializing, deserializing and merging call graphs."""
    temp_dir = Path(tempfile.mkdtemp('gcmole_test'))

    call_graph1 = self.create_callgraph(
        OutputLines('B → C D E', 'D →'), OutputLines('A → B C'))
    self.assertDictEqual(
        call_graph1.funcs,
        {'A': set(), 'B': set('A'), 'C': set(['A', 'B']), 'D': set('B'),
         'E': set('B')})

    call_graph2 = self.create_callgraph(
        OutputLines('E → A'), OutputLines('C → D F'))
    self.assertDictEqual(
        call_graph2.funcs,
        {'A': set('E'), 'C': set(), 'D': set('C'), 'E': set(), 'F': set('C')})

    file1 = temp_dir / 'file1.bin'
    file2 = temp_dir / 'file2.bin'
    call_graph1.to_file(file1)
    call_graph2.to_file(file2)

    expected = {'A': set(['E']), 'B': set('A'), 'C': set(['A', 'B']),
                'D': set(['B', 'C']), 'E': set(['B']), 'F': set(['C'])}

    call_graph = gcmole.CallGraph.from_files(file1, file2)
    self.assertDictEqual(call_graph.funcs, expected)

    call_graph = gcmole.CallGraph.from_files(file2, file1)
    self.assertDictEqual(call_graph.funcs, expected)

    call_graph3 = self.create_callgraph(
        OutputLines('F → G'), OutputLines('G →'))
    self.assertDictEqual(
        call_graph3.funcs,
        {'G': set('F'), 'F': set()})

    file3 = temp_dir / 'file3.bin'
    call_graph3.to_file(file3)

    call_graph = gcmole.CallGraph.from_files(file1, file2, file3)
    self.assertDictEqual(call_graph.funcs, dict(G=set('F'), **expected))

  def testClassHierarchyPackaging(self):
    temp_dir = Path(tempfile.mkdtemp('gcmole_test'))

    cg1 = gcmole.CallGraph()
    cg1.funcs['A'] = set()
    cg1.class_hierarchies = {"ClassA": {"bases": [], "overrides": {}}}

    cg2 = gcmole.CallGraph()
    cg2.funcs['B'] = set()
    cg2.class_hierarchies = {"ClassB": {"bases": ["ClassA"], "overrides": {}}}

    file1 = temp_dir / 'file1.bin'
    file2 = temp_dir / 'file2.bin'
    cg1.to_file(file1)
    cg2.to_file(file2)

    merged = gcmole.CallGraph.from_files(file1, file2)
    self.assertEqual(len(merged.class_hierarchies), 2)
    self.assertDictEqual(
        merged.class_hierarchies, {
            "ClassA": {
                "bases": [],
                "overrides": {},
                "virtual_methods": []
            },
            "ClassB": {
                "bases": ["ClassA"],
                "overrides": {},
                "virtual_methods": []
            }
        })

  def create_collector(self, outputs):
    Options = collections.namedtuple('OptionsForCollector', ['allowlist'])
    options = Options(True)
    call_graph = self.create_callgraph(*outputs)
    collector = gcmole.GCSuspectsCollector(options, call_graph.funcs)
    collector.propagate()
    return collector

  def check(self, outputs, expected_gc, expected_gc_caused):
    """Verify the GCSuspectsCollector propagation and outputs against test
    data.

    Args:
      outputs: List of OutputLines object simulating the lines returned by
          the GCMole plugin in drop-callees mode. Each output lines object
          represents one plugin invocation.
      expected_gc: Mapping as expected by GCSuspectsCollector.gc.
      expected_gc_caused: Mapping as expected by GCSuspectsCollector.gc_caused.
    """
    collector = self.create_collector(outputs)
    self.assertDictEqual(collector.gc, expected_gc)
    self.assertDictEqual(collector.gc_caused, expected_gc_caused)

  def testNoGC(self):
    self.check(
        outputs=[OutputLines()],
        expected_gc={},
        expected_gc_caused={},
    )
    self.check(
        outputs=[OutputLines('A →')],
        expected_gc={},
        expected_gc_caused={},
    )
    self.check(
        outputs=[OutputLines('A →', 'B →')],
        expected_gc={},
        expected_gc_caused={},
    )
    self.check(
        outputs=[OutputLines('A → B C')],
        expected_gc={},
        expected_gc_caused={},
    )
    self.check(
        outputs=[OutputLines('A → B', 'B → C')],
        expected_gc={},
        expected_gc_caused={},
    )
    self.check(
        outputs=[OutputLines('A → B C', 'B → D', 'D → A', 'C →')],
        expected_gc={},
        expected_gc_caused={},
    )
    self.check(
        outputs=[OutputLines('A →'), OutputLines('B →')],
        expected_gc={},
        expected_gc_caused={},
    )
    self.check(
        outputs=[OutputLines('A → B'), OutputLines('B → C')],
        expected_gc={},
        expected_gc_caused={},
    )
    self.check(
        outputs=[OutputLines('A → B C'),
                 OutputLines('B → D', 'D → A'),
                 OutputLines('C →')],
        expected_gc={},
        expected_gc_caused={},
    )

  def testGCOneFile(self):
    self.check(
        outputs=[OutputLines(f'{GC} →')],
        expected_gc={GC: True},
        expected_gc_caused={GC: {'<GC>'}},
    )
    self.check(
        outputs=[OutputLines(f'A → {GC}')],
        expected_gc={GC: True, 'A': True},
        expected_gc_caused={'A': {GC}, GC: {'<GC>'}},
    )
    self.check(
        outputs=[OutputLines(f'A → {GC}', 'B → A')],
        expected_gc={GC: True, 'A': True, 'B': True},
        expected_gc_caused={'B': {'A'}, 'A': {GC}, GC: {'<GC>'}},
    )
    self.check(
        outputs=[OutputLines('B → A', f'A → {GC}')],
        expected_gc={GC: True, 'A': True, 'B': True},
        expected_gc_caused={'B': {'A'}, 'A': {GC}, GC: {'<GC>'}},
    )
    self.check(
        outputs=[OutputLines(f'A → B {GC}', 'B →', 'C → B A')],
        expected_gc={GC: True, 'A': True, 'C': True},
        expected_gc_caused={'C': {'A'}, 'A': {GC}, GC: {'<GC>'}},
    )
    self.check(
        outputs=[OutputLines(f'A → {GC}', 'B → A', 'C → A', 'D → B C')],
        expected_gc={GC: True, 'A': True, 'B': True, 'C': True, 'D': True},
        expected_gc_caused={'C': {'A'}, 'A': {GC}, 'B': {'A'}, 'D': {'B', 'C'},
                            GC: {'<GC>'}},
    )

  def testAllowListOneFile(self):
    self.check(
        outputs=[OutputLines(f'{WF} →')],
        expected_gc={WF: False},
        expected_gc_caused={},
    )
    self.check(
        outputs=[OutputLines(f'{WF} → {GC}')],
        expected_gc={GC: True, WF: False},
        expected_gc_caused={WF: {GC}, GC: {'<GC>'}},
    )
    self.check(
        outputs=[OutputLines(f'A → {GC}', f'{WF} → A B', 'D → A B',
                             f'E → {WF}')],
        expected_gc={GC: True, WF: False, 'A': True, 'D': True},
        expected_gc_caused={'A': {GC}, WF: {'A'}, 'D': {'A'}, GC: {'<GC>'}},
    )

  def testSafepointOneFile(self):
    self.check(
        outputs=[OutputLines(f'{SP} →')],
        expected_gc={SP: True},
        expected_gc_caused={SP: {'<Safepoint>'}},
    )
    self.check(
        outputs=[OutputLines('B → A', f'A → {SP}')],
        expected_gc={SP: True, 'A': True, 'B': True},
        expected_gc_caused={'B': {'A'}, 'A': {SP}, SP: {'<Safepoint>'}},
    )

  def testCombinedOneFile(self):
    self.check(
        outputs=[OutputLines(f'{GC} →', f'{SP} →')],
        expected_gc={SP: True, GC: True},
        expected_gc_caused={SP: {'<Safepoint>'}, GC: {'<GC>'}},
    )
    self.check(
        outputs=[OutputLines(f'A → {GC}', f'B → {SP}')],
        expected_gc={GC: True, SP: True, 'A': True, 'B': True},
        expected_gc_caused={'B': {SP}, 'A': {GC}, SP: {'<Safepoint>'},
                            GC: {'<GC>'}},
    )
    self.check(
        outputs=[OutputLines(f'A → {GC}', f'B → {SP}', 'C → D A B')],
        expected_gc={GC: True, SP: True, 'A': True, 'B': True, 'C': True},
        expected_gc_caused={'B': {SP}, 'A': {GC}, 'C': {'A', 'B'},
                            SP: {'<Safepoint>'}, GC: {'<GC>'}},
    )

  def testCombinedMoreFiles(self):
    self.check(
        outputs=[OutputLines(f'A → {GC}'), OutputLines(f'B → {SP}')],
        expected_gc={GC: True, SP: True, 'A': True, 'B': True},
        expected_gc_caused={'B': {SP}, 'A': {GC}, SP: {'<Safepoint>'},
                            GC: {'<GC>'}},
    )
    self.check(
        outputs=[OutputLines(f'A → {GC}'), OutputLines(f'B → {SP}'),
                 OutputLines('C → D A B')],
        expected_gc={GC: True, SP: True, 'A': True, 'B': True, 'C': True},
        expected_gc_caused={'B': {SP}, 'A': {GC}, 'C': {'A', 'B'},
                            SP: {'<Safepoint>'}, GC: {'<GC>'}},
    )

  def testWriteGCMoleResults(self):
    temp_dir = Path(tempfile.mkdtemp('gcmole_test'))
    Options = collections.namedtuple('OptionsForWriting', ['v8_target_cpu'])
    collector = self.create_collector(
        [OutputLines(f'A → {GC}'), OutputLines(f'B → {SP}')])
    gcmole.write_gcmole_results(collector, Options('x64'), temp_dir)

    gcsuspects_expected = textwrap.dedent(f"""\
      {GC}
      {SP}
      A
      B
    """)

    with open(temp_dir / 'gcsuspects') as f:
      self.assertEqual(f.read(), gcsuspects_expected)

    gccauses_expected = textwrap.dedent(f"""
      {GC}
      start,nested
      <GC>
      end,nested
      {SP}
      start,nested
      <Safepoint>
      end,nested
      A
      start,nested
      {GC}
      end,nested
      B
      start,nested
      {SP}
      end,nested
    """).strip()

    with open(temp_dir / 'gccauses') as f:
      self.assertEqual(f.read().strip(), gccauses_expected)


class ArgsTest(unittest.TestCase):

  def testArgs(self):
    """Test argument retrieval using a fake v8 file system and build dir."""
    with tempfile.TemporaryDirectory('gcmole_args_test') as temp_dir:
      temp_dir = Path(temp_dir)
      temp_out = temp_dir / 'out'
      temp_gcmole = temp_dir / 'tools' / 'gcmole' / 'gcmole_args.py'

      shutil.copytree(abs_test_file('out'), temp_out)
      os.makedirs(temp_gcmole.parent)
      shutil.copy(GCMOLE_PATH / 'gcmole_args.py', temp_gcmole)

      # Simulate a ninja call relative to the build dir.
      gn_sysroot = '//build/linux/debian_bullseye_amd64-sysroot'
      subprocess.check_call(
          [sys.executable, temp_gcmole, gn_sysroot], cwd=temp_out)

      with open(temp_dir / 'out' / 'v8_gcmole.args') as f:
        self.assertEqual(f.read().split(), [
            '-DUSE_GLIB=1',
            '-DV8_TARGET_ARCH_X64',
            '-I.',
            '-Iout/gen',
            '-Iinclude',
            '-Iout/gen/include',
            '-isystem../buildtools/third_party/libc++/trunk/include',
            '--sysroot=build/linux/debian_bullseye_amd64-sysroot',
        ])


class DevirtualizationTest(unittest.TestCase):

  def setUp(self):
    self.temp_dir = Path(tempfile.mkdtemp('gcmole_devirt_test'))

  def tearDown(self):
    shutil.rmtree(self.temp_dir)

  def write_hierarchy(self, filename, hierarchy_data):
    import json
    filepath = self.temp_dir / f"class_hierarchy_{filename}.json"
    with open(filepath, 'w') as f:
      json.dump(hierarchy_data, f)

  def test_delimiter_collision(self):
    self.write_hierarchy(
        'test', {
            'Visitor::ON::Heap': {
                'bases': ['Base'],
                'overrides': {},
                'virtual_methods': []
            }
        })

    callgraph = gcmole.CallGraph()
    callgraph.funcs['Base::Visit,Base::Visit'] = set()
    synth_key = 'SYNTHOVER-Visitor::ON::Heap-Base::Visit,Visitor::ON::Heap::Visit'
    callgraph.funcs[synth_key] = set()

    node = gcmole.SynthesizedNode(synth_key)
    self.assertTrue(node.is_valid)
    self.assertEqual(node.derived_class_qualified, 'Visitor::ON::Heap')
    self.assertEqual(node.base_method_mangled, 'Base::Visit')

  def test_transitive_overrides(self):
    self.write_hierarchy(
        'test', {
            'A': {
                'bases': [],
                'overrides': {},
                'virtual_methods': ['A_foo', 'A_bar']
            },
            'B': {
                'bases': ['A'],
                'overrides': {
                    'A_bar': 'B_bar'
                },
                'virtual_methods': ['B_bar']
            },
            'C': {
                'bases': ['B'],
                'overrides': {
                    'B_bar': 'C_bar'
                },
                'virtual_methods': ['C_bar']
            }
        })

    callgraph = gcmole.CallGraph()
    # A_foo calls A_bar virtually on 'this'
    callgraph.funcs['VIRTUALTHIS-A-A_bar,A::bar'] = {'A_foo,A::foo'}
    callgraph.funcs['A_foo,A::foo'] = set()

    # We want to simulate calling A_foo on B receiver virtually
    callgraph.funcs['VIRTUAL-B-A_foo,B::foo'] = {'Caller'}
    # We want to simulate calling A_foo on C receiver virtually
    callgraph.funcs['VIRTUAL-C-A_foo,C::foo'] = {'Caller'}

    callgraph.funcs['B_bar,B::bar'] = set()
    callgraph.funcs['C_bar,C::bar'] = set()

    TestOptions = collections.namedtuple('TestOptions', ['out_dir'])
    opts = TestOptions(self.temp_dir)

    gcmole.synthesize_virtual_links(callgraph, opts)

    synth_key_b = 'SYNTHOVER-B-A_foo,B::foo'
    synth_key_c = 'SYNTHOVER-C-A_foo,C::foo'

    # Check synthetic overrides are created and linked to VIRTUAL- callsites
    self.assertIn('VIRTUAL-B-A_foo,B::foo', callgraph.funcs[synth_key_b])
    self.assertIn('VIRTUAL-C-A_foo,C::foo', callgraph.funcs[synth_key_c])

    # Check VIRTUALTHIS specialized for B is created and called by synth_key_b
    vthis_b = 'VIRTUALTHIS-B-A_bar,B::bar'
    self.assertIn(vthis_b, callgraph.funcs)
    self.assertIn(synth_key_b, callgraph.funcs[vthis_b])

    # Check VIRTUALTHIS specialized for B resolves to B_bar and C_bar
    self.assertIn(vthis_b, callgraph.funcs['B_bar,B::bar'])
    self.assertIn(vthis_b, callgraph.funcs['C_bar,C::bar'])

    # Check VIRTUALTHIS specialized for C is created and called by synth_key_c
    vthis_c = 'VIRTUALTHIS-C-A_bar,C::bar'
    self.assertIn(vthis_c, callgraph.funcs)
    self.assertIn(synth_key_c, callgraph.funcs[vthis_c])

    # Check VIRTUALTHIS specialized for C resolves to C_bar, but NOT B_bar
    self.assertIn(vthis_c, callgraph.funcs['C_bar,C::bar'])
    self.assertNotIn(vthis_c, callgraph.funcs['B_bar,B::bar'])

  def test_entry_unoverridden_descendants(self):
    self.write_hierarchy(
        'test', {
            'A': {
                'bases': [],
                'overrides': {},
                'virtual_methods': ['A_bar']
            },
            'B': {
                'bases': ['A'],
                'overrides': {},
                'virtual_methods': []
            },
            'C': {
                'bases': ['B'],
                'overrides': {
                    'A_bar': 'C_bar'
                },
                'virtual_methods': ['C_bar']
            }
        })

    callgraph = gcmole.CallGraph()
    # Base method must be in callgraph to resolve qualified name for synthetic overrides
    callgraph.funcs['A_bar,A::bar'] = set()
    # Virtual call to A_bar on B receiver
    callgraph.funcs['VIRTUAL-B-A_bar,B::bar'] = {'Caller'}
    callgraph.funcs['C_bar,C::bar'] = set()

    TestOptions = collections.namedtuple('TestOptions', ['out_dir'])
    opts = TestOptions(self.temp_dir)

    gcmole.synthesize_virtual_links(callgraph, opts)

    synth_key = 'SYNTHOVER-B-A_bar,B::bar'
    # The virtual callsite should link to both the active impl (synthetic) and the descendant override
    self.assertIn('VIRTUAL-B-A_bar,B::bar', callgraph.funcs[synth_key])
    self.assertIn('VIRTUAL-B-A_bar,B::bar', callgraph.funcs['C_bar,C::bar'])

  def test_unoverridden_virtual_methods_chain(self):
    self.write_hierarchy(
        'test', {
            'A': {
                'bases': [],
                'overrides': {},
                'virtual_methods': ['A_bar', 'A_qux', 'A_baz']
            },
            'B': {
                'bases': ['A'],
                'overrides': {
                    'A_baz': 'B_baz'
                },
                'virtual_methods': ['B_baz']
            }
        })

    callgraph = gcmole.CallGraph()
    # A_bar -> A_qux (virtual self-call)
    callgraph.funcs['VIRTUALTHIS-A-A_qux,A::qux'] = {'A_bar,A::bar'}
    callgraph.funcs['A_bar,A::bar'] = set()
    # A_qux -> A_baz (virtual self-call)
    callgraph.funcs['VIRTUALTHIS-A-A_baz,A::baz'] = {'A_qux,A::qux'}
    callgraph.funcs['A_qux,A::qux'] = set()
    callgraph.funcs['A_baz,A::baz'] = set()

    # Simulate virtual call to A_bar on B receiver
    callgraph.funcs['VIRTUAL-B-A_bar,B::bar'] = {'Caller'}
    callgraph.funcs['B_baz,B::baz'] = set()

    TestOptions = collections.namedtuple('TestOptions', ['out_dir'])
    opts = TestOptions(self.temp_dir)

    gcmole.synthesize_virtual_links(callgraph, opts)

    synth_key_bar = 'SYNTHOVER-B-A_bar,B::bar'
    synth_key_qux = 'SYNTHOVER-B-A_qux,B::qux'

    # Check synthetic overrides are created
    self.assertIn(synth_key_bar, callgraph.funcs)
    self.assertIn(synth_key_qux, callgraph.funcs)

    # Check SYNTHOVER-B-A_bar calls VIRTUALTHIS-B-A_qux
    vthis_qux = 'VIRTUALTHIS-B-A_qux,B::qux'
    self.assertIn(synth_key_bar, callgraph.funcs[vthis_qux])

    # Check VIRTUALTHIS-B-A_qux resolves to SYNTHOVER-B-A_qux
    self.assertIn(vthis_qux, callgraph.funcs[synth_key_qux])

    # Check SYNTHOVER-B-A_qux calls VIRTUALTHIS-B-A_baz
    vthis_baz = 'VIRTUALTHIS-B-A_baz,B::baz'
    self.assertIn(synth_key_qux, callgraph.funcs[vthis_baz])

    # Check VIRTUALTHIS-B-A_baz resolves to B_baz
    self.assertIn(vthis_baz, callgraph.funcs['B_baz,B::baz'])

  def test_multiple_inheritance(self):
    self.write_hierarchy(
        'mi_test', {
            'BaseA': {
                'bases': [],
                'overrides': {},
                'virtual_methods': ['BaseA_foo']
            },
            'BaseB': {
                'bases': [],
                'overrides': {},
                'virtual_methods': ['BaseB_bar']
            },
            'Derived': {
                'bases': ['BaseA', 'BaseB'],
                'overrides': {
                    'BaseA_foo': 'Derived_foo',
                    'BaseB_bar': 'Derived_bar'
                },
                'virtual_methods': ['Derived_foo', 'Derived_bar']
            }
        })

    callgraph = gcmole.CallGraph()
    callgraph.funcs['BaseA_foo,BaseA::foo'] = {'CallerA'}
    callgraph.funcs['BaseB_bar,BaseB::bar'] = {'CallerB'}
    callgraph.funcs['Derived_foo,Derived::foo'] = set()
    callgraph.funcs['Derived_bar,Derived::bar'] = set()

    synth_key_a = 'VIRTUAL-Derived-BaseA_foo,Derived::foo'
    synth_key_b = 'VIRTUAL-Derived-BaseB_bar,Derived::bar'
    callgraph.funcs[synth_key_a] = {'CallerA'}
    callgraph.funcs[synth_key_b] = {'CallerB'}

    TestOptions = collections.namedtuple('TestOptions', ['out_dir'])
    opts = TestOptions(self.temp_dir)

    gcmole.synthesize_virtual_links(callgraph, opts)

    self.assertIn(synth_key_a, callgraph.funcs['Derived_foo,Derived::foo'])
    self.assertIn(synth_key_b, callgraph.funcs['Derived_bar,Derived::bar'])

  def test_diamond_inheritance(self):
    self.write_hierarchy(
        'diamond_test', {
            'A': {
                'bases': [],
                'overrides': {},
                'virtual_methods': ['A_foo']
            },
            'B': {
                'bases': ['A'],
                'overrides': {
                    'A_foo': 'B_foo'
                },
                'virtual_methods': ['B_foo']
            },
            'C': {
                'bases': ['A'],
                'overrides': {
                    'A_foo': 'C_foo'
                },
                'virtual_methods': ['C_foo']
            },
            'D': {
                'bases': ['B', 'C'],
                'overrides': {
                    'B_foo': 'D_foo',
                    'C_foo': 'D_foo'
                },
                'virtual_methods': ['D_foo']
            }
        })

    callgraph = gcmole.CallGraph()
    callgraph.funcs['D_foo,D::foo'] = set()

    # Virtual callsite on D receiver
    synth_key = 'VIRTUAL-D-A_foo,D::foo'
    callgraph.funcs[synth_key] = {'Caller'}

    TestOptions = collections.namedtuple('TestOptions', ['out_dir'])
    opts = TestOptions(self.temp_dir)

    gcmole.synthesize_virtual_links(callgraph, opts)

    # D transitively overrides A_foo, so it should link directly (no synthetic override needed)
    self.assertIn(synth_key, callgraph.funcs['D_foo,D::foo'])

  def test_overloaded_virtual_methods(self):
    self.write_hierarchy(
        'overload_test', {
            'Base': {
                'bases': [],
                'overrides': {},
                'virtual_methods': ['Base_Visit_Int', 'Base_Visit_Double']
            },
            'Derived': {
                'bases': ['Base'],
                'overrides': {
                    'Base_Visit_Int': 'Derived_Visit_Int',
                    'Base_Visit_Double': 'Derived_Visit_Double'
                },
                'virtual_methods':
                    ['Derived_Visit_Int', 'Derived_Visit_Double']
            }
        })

    callgraph = gcmole.CallGraph()
    callgraph.funcs['Base_Visit_Int,Base::Visit'] = {'CallerInt'}
    callgraph.funcs['Base_Visit_Double,Base::Visit'] = {'CallerDouble'}
    callgraph.funcs['Derived_Visit_Int,Derived::Visit'] = set()
    callgraph.funcs['Derived_Visit_Double,Derived::Visit'] = set()

    synth_key_int = 'VIRTUAL-Derived-Base_Visit_Int,Derived::Visit'
    synth_key_double = 'VIRTUAL-Derived-Base_Visit_Double,Derived::Visit'
    callgraph.funcs[synth_key_int] = {'CallerInt'}
    callgraph.funcs[synth_key_double] = {'CallerDouble'}

    TestOptions = collections.namedtuple('TestOptions', ['out_dir'])
    opts = TestOptions(self.temp_dir)

    gcmole.synthesize_virtual_links(callgraph, opts)

    self.assertIn(synth_key_int,
                  callgraph.funcs['Derived_Visit_Int,Derived::Visit'])
    self.assertNotIn(synth_key_int,
                     callgraph.funcs['Derived_Visit_Double,Derived::Visit'])
    self.assertIn(synth_key_double,
                  callgraph.funcs['Derived_Visit_Double,Derived::Visit'])
    self.assertNotIn(synth_key_double,
                     callgraph.funcs['Derived_Visit_Int,Derived::Visit'])

  def test_templated_class_sanitization(self):
    self.write_hierarchy(
        'temp_test', {
            'MyTemplate<int>': {
                'bases': ['Base'],
                'overrides': {
                    'Base_foo': 'MyTemplate_foo'
                },
                'virtual_methods': ['MyTemplate_foo']
            }
        })

    callgraph = gcmole.CallGraph()
    callgraph.funcs['Base_foo,Base::foo'] = {'Caller'}
    callgraph.funcs['MyTemplate_foo,MyTemplate<int>::foo'] = set()

    synth_key = 'VIRTUAL-MyTemplate<int>-Base_foo,MyTemplate<int>::foo'
    callgraph.funcs[synth_key] = {'Caller'}

    TestOptions = collections.namedtuple('TestOptions', ['out_dir'])
    opts = TestOptions(self.temp_dir)

    gcmole.synthesize_virtual_links(callgraph, opts)

    # Check that it correctly resolved despite template brackets
    self.assertIn(synth_key,
                  callgraph.funcs['MyTemplate_foo,MyTemplate<int>::foo'])

  def test_safe_override_call(self):
    self.write_hierarchy(
        'safe_test', {
            'Base': {
                'bases': [],
                'overrides': {},
                'virtual_methods': ['Base_foo']
            },
            'DerivedUnsafe': {
                'bases': ['Base'],
                'overrides': {
                    'Base_foo': 'DerivedUnsafe_foo'
                },
                'virtual_methods': ['DerivedUnsafe_foo']
            },
            'DerivedSafe': {
                'bases': ['Base'],
                'overrides': {
                    'Base_foo': 'DerivedSafe_foo'
                },
                'virtual_methods': ['DerivedSafe_foo']
            },
            'SubDerivedSafe': {
                'bases': ['DerivedSafe'],
                'overrides': {},
                'virtual_methods': []
            }
        })

    callgraph = gcmole.CallGraph()
    # We use the new VIRTUAL- nodes to represent virtual callsites.
    callgraph.funcs['VIRTUAL-Base-Base_foo,Base::foo'] = {'CallerBase'}
    callgraph.funcs['CallerBase'] = set()

    callgraph.funcs['VIRTUAL-DerivedUnsafe-Base_foo,DerivedUnsafe::foo'] = {
        'CallerUnsafe'
    }
    callgraph.funcs['CallerUnsafe'] = set()

    callgraph.funcs['VIRTUAL-DerivedSafe-Base_foo,DerivedSafe::foo'] = {
        'CallerSafe'
    }
    callgraph.funcs['CallerSafe'] = set()

    callgraph.funcs['VIRTUAL-SubDerivedSafe-Base_foo,SubDerivedSafe::foo'] = {
        'CallerSubSafe'
    }
    callgraph.funcs['CallerSubSafe'] = set()

    # Concrete implementations
    callgraph.funcs['Base_foo,Base::foo'] = set()
    callgraph.funcs['DerivedUnsafe_foo,DerivedUnsafe::foo'] = set()
    callgraph.funcs['DerivedSafe_foo,DerivedSafe::foo'] = set()

    # DerivedUnsafe_foo causes GC
    callgraph.funcs['CollectGarbage,CollectGarbage'] = {
        'DerivedUnsafe_foo,DerivedUnsafe::foo'
    }

    TestOptions = collections.namedtuple(
        'TestOptions', ['out_dir', 'allowlist', 'v8_root_dir', 'v8_target_cpu'])
    opts = TestOptions(self.temp_dir, True, self.temp_dir, 'x64')

    gcmole.generate_gc_suspects_from_callgraph(callgraph, opts)

    collector = gcmole.GCSuspectsCollector(opts, callgraph.funcs)
    collector.propagate()

    # Virtual call on Base receiver can go to DerivedUnsafe_foo, so it is GC.
    self.assertTrue(collector.gc.get('VIRTUAL-Base-Base_foo,Base::foo', False))
    # Virtual call on DerivedUnsafe receiver goes to DerivedUnsafe_foo, so it is GC.
    self.assertTrue(
        collector.gc.get('VIRTUAL-DerivedUnsafe-Base_foo,DerivedUnsafe::foo',
                         False))
    # Virtual call on DerivedSafe receiver goes to DerivedSafe_foo (safe) and
    # SubDerivedSafe::foo (safe), so it is NOT GC.
    self.assertFalse(
        collector.gc.get('VIRTUAL-DerivedSafe-Base_foo,DerivedSafe::foo',
                         False))
    # Virtual call on SubDerivedSafe receiver goes to SubDerivedSafe::foo (safe),
    # so it is NOT GC.
    self.assertFalse(
        collector.gc.get('VIRTUAL-SubDerivedSafe-Base_foo,SubDerivedSafe::foo',
                         False))

    # The concrete implementations themselves:
    # Base_foo is safe (it doesn't call anything unsafe in its body).
    self.assertFalse(collector.gc.get('Base_foo,Base::foo', False))
    # DerivedUnsafe_foo is GC (calls CollectGarbage).
    self.assertTrue(
        collector.gc.get('DerivedUnsafe_foo,DerivedUnsafe::foo', False))
    # DerivedSafe_foo is safe.
    self.assertFalse(
        collector.gc.get('DerivedSafe_foo,DerivedSafe::foo', False))
    # Synthetic override is safe.
    synth_key = 'SYNTHOVER-SubDerivedSafe-Base_foo,SubDerivedSafe::foo'
    self.assertFalse(collector.gc.get(synth_key, False))


if __name__ == '__main__':
  unittest.main()
