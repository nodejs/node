// Flags: --expose-internals
'use strict';
const common = require('../common');
const assert = require('node:assert');
const { TapParser } = require('internal/test_runner/tap_parser');
const { TapChecker } = require('internal/test_runner/tap_checker');

const cases = [
  {
    input: 'TAP version 13',
    expected: [
      {
        nesting: 0,
        kind: 'VersionKeyword',
        node: { version: '13' },
        lexeme: 'TAP version 13',
      },
    ],
  },
  {
    input: '123',
    expected: [
      {
        kind: 'Unknown',
        node: { value: '123' },
        nesting: 0,
        lexeme: '123',
      },
    ],
  },
  {
    input: '# 123',
    expected: [
      {
        kind: 'Comment',
        node: { comment: '123' },
        nesting: 0,
        lexeme: '# 123',
      },
    ],
  },
  {
    input: '1..',
    expected: [
      {
        kind: 'Unknown',
        node: { value: '1..' },
        nesting: 0,
        lexeme: '1..',
      },
    ],
  },
  {
    input: '1..abc',
    expected: [
      {
        kind: 'Unknown',
        node: { value: '1..abc' },
        nesting: 0,
        lexeme: '1..abc',
      },
    ],
  },
  {
    input: '1..-1',
    expected: [
      {
        kind: 'Unknown',
        node: { value: '1..-1' },
        nesting: 0,
        lexeme: '1..-1',
      },
    ],
  },
  {
    input: '1.1',
    expected: [
      {
        kind: 'Unknown',
        node: { value: '1.1' },
        nesting: 0,
        lexeme: '1.1',
      },
    ],
  },
  {
    input: '1.....4',
    expected: [
      {
        kind: 'Unknown',
        node: { value: '1.....4' },
        nesting: 0,
        lexeme: '1.....4',
      },
    ],
  },
  {
    input: 'TAP 12',
    expected: [
      {
        kind: 'Unknown',
        node: { value: 'TAP 12' },
        nesting: 0,
        lexeme: 'TAP 12',
      },
    ],
  },
  {
    input: 'TAP version',
    expected: [
      {
        kind: 'Unknown',
        node: { value: 'TAP version' },
        nesting: 0,
        lexeme: 'TAP version',
      },
    ],
  },
  {
    input: 'TAP version v14',
    expected: [
      {
        kind: 'Unknown',
        node: { value: 'TAP version v14' },
        nesting: 0,
        lexeme: 'TAP version v14',
      },
    ],
  },
  {
    input: 'TAP TAP TAP',
    expected: [
      {
        kind: 'Unknown',
        node: { value: 'TAP TAP TAP' },
        nesting: 0,
        lexeme: 'TAP TAP TAP',
      },
    ],
  },
  {
    input: '--- yaml',
    expected: [
      {
        kind: 'Unknown',
        node: { value: '--- yaml' },
        nesting: 0,
        lexeme: '--- yaml',
      },
    ],
  },
  {
    input: '... ... yaml',
    expected: [
      {
        kind: 'Unknown',
        node: { value: '... ... yaml' },
        nesting: 0,
        lexeme: '... ... yaml',
      },
    ],
  },
  {
    input: 'ook 1',
    expected: [
      {
        kind: 'Unknown',
        node: { value: 'ook 1' },
        nesting: 0,
        lexeme: 'ook 1',
      },
    ],
  },
  {
    input: '   ok 98',
    expected: [
      {
        kind: 'Unknown',
        node: { value: '   ok 98' },
        nesting: 0,
        lexeme: '   ok 98',
      },
    ],
  },
  {
    input: 'pragma ++++++',
    expected: [
      {
        kind: 'Unknown',
        node: { value: 'pragma ++++++' },
        nesting: 0,
        lexeme: 'pragma ++++++',
      },
    ],
  },
  {
    input: 'Bailout!',
    expected: [
      {
        kind: 'Unknown',
        node: { value: 'Bailout!' },
        nesting: 0,
        lexeme: 'Bailout!',
      },
    ],
  },
  {
    input: 'invalid tap',
    expected: [
      {
        nesting: 0,
        kind: 'Unknown',
        node: { value: 'invalid tap' },
        lexeme: 'invalid tap',
      },
    ],
  },
  {
    input: 'TAP version 13\ninvalid tap after harness',
    expected: [
      {
        nesting: 0,
        kind: 'VersionKeyword',
        node: { version: '13' },
        lexeme: 'TAP version 13',
      },
      {
        nesting: 0,
        kind: 'Unknown',
        node: { value: 'invalid tap after harness' },
        lexeme: 'invalid tap after harness',
      },
    ],
  },
  {
    input: `TAP version 13
    # nested diagnostic
# diagnostic comment`,
    expected: [
      {
        nesting: 0,
        kind: 'VersionKeyword',
        node: { version: '13' },
        lexeme: 'TAP version 13',
      },
      {
        nesting: 1,
        kind: 'Comment',
        node: { comment: 'nested diagnostic' },
        lexeme: '    # nested diagnostic',
      },
      {
        nesting: 0,
        kind: 'Comment',
        node: { comment: 'diagnostic comment' },
        lexeme: '# diagnostic comment',
      },
    ],
  },
  {
    input: `TAP version 13
    1..5
1..3
2..2`,
    expected: [
      {
        nesting: 0,
        kind: 'VersionKeyword',
        node: { version: '13' },
        lexeme: 'TAP version 13',
      },
      {
        nesting: 1,
        kind: 'PlanKeyword',
        node: { start: '1', end: '5' },
        lexeme: '    1..5',
      },
      {
        nesting: 0,
        kind: 'PlanKeyword',
        node: { start: '1', end: '3' },
        lexeme: '1..3',
      },
      {
        nesting: 0,
        kind: 'PlanKeyword',
        node: { start: '2', end: '2' },
        lexeme: '2..2',
      },
    ],
  },
  {
    input: `TAP version 13
ok 1 - test
ok 2 - test # SKIP
not ok 3 - test # TODO reason`,
    expected: [
      {
        nesting: 0,
        kind: 'VersionKeyword',
        node: { version: '13' },
        lexeme: 'TAP version 13',
      },
      {
        nesting: 0,
        kind: 'TestPointKeyword',
        node: {
          status: { fail: false, pass: true, todo: false, skip: false },
          id: '1',
          description: 'test',
          reason: '',
          time: 0,
          diagnostics: [],
        },
        lexeme: 'ok 1 - test',
      },
      {
        nesting: 0,
        kind: 'TestPointKeyword',
        node: {
          status: { fail: false, pass: true, todo: false, skip: true },
          id: '2',
          description: 'test',
          reason: '',
          time: 0,
          diagnostics: [],
        },
        lexeme: 'ok 2 - test # SKIP',
      },
      {
        nesting: 0,
        kind: 'TestPointKeyword',
        node: {
          status: { fail: true, pass: false, todo: true, skip: false },
          id: '3',
          description: 'test',
          reason: 'reason',
          time: 0,
          diagnostics: [],
        },
        lexeme: 'not ok 3 - test # TODO reason',
      },
    ],
  },
  {
    input: `TAP version 13
# Subtest: test
ok 1 - test
ok 2 - test`,
    expected: [
      {
        nesting: 0,
        kind: 'VersionKeyword',
        node: { version: '13' },
        lexeme: 'TAP version 13',
      },
      {
        nesting: 0,
        kind: 'SubTestPointKeyword',
        node: { name: 'test' },
        lexeme: '# Subtest: test',
      },
      {
        nesting: 0,
        kind: 'TestPointKeyword',
        node: {
          status: { fail: false, pass: true, todo: false, skip: false },
          id: '1',
          description: 'test',
          reason: '',
          time: 0,
          diagnostics: [],
        },
        lexeme: 'ok 1 - test',
      },
      {
        nesting: 0,
        kind: 'TestPointKeyword',
        node: {
          status: { fail: false, pass: true, todo: false, skip: false },
          id: '2',
          description: 'test',
          reason: '',
          time: 0,
          diagnostics: [],
        },
        lexeme: 'ok 2 - test',
      },
    ],
  },
  {
    input: `TAP version 13
# Subtest: test
ok 1 - test
  ---
  foo: bar
  duration_ms: 0.0001
  prop: |-
    multiple
    lines
  ...`,
    expected: [
      {
        nesting: 0,
        kind: 'VersionKeyword',
        node: { version: '13' },
        lexeme: 'TAP version 13',
      },
      {
        nesting: 0,
        kind: 'SubTestPointKeyword',
        node: { name: 'test' },
        lexeme: '# Subtest: test',
      },
      {
        nesting: 0,
        kind: 'TestPointKeyword',
        node: {
          status: { fail: false, pass: true, todo: false, skip: false },
          id: '1',
          description: 'test',
          reason: '',
          time: 0.0001,
          diagnostics: [
            'foo: bar',
            'duration_ms: 0.0001',
            'prop: |-',
            '  multiple',
            '  lines',
          ],
        },
        lexeme: 'ok 1 - test',
      },
    ],
  },
  {
    input: `TAP version 13
# Subtest: test/fixtures/test-runner/index.test.js
    # Subtest: this should pass
    ok 1 - this should pass
      ---
      duration_ms: 0.0001
      ...
    1..1`,
    expected: [
      {
        nesting: 0,
        kind: 'VersionKeyword',
        node: { version: '13' },
        lexeme: 'TAP version 13',
      },
      {
        kind: 'SubTestPointKeyword',
        lexeme: '# Subtest: test/fixtures/test-runner/index.test.js',
        nesting: 0,
        node: {
          name: 'test/fixtures/test-runner/index.test.js',
        },
      },
      {
        kind: 'SubTestPointKeyword',
        lexeme: '    # Subtest: this should pass',
        nesting: 1,
        node: {
          name: 'this should pass',
        },
      },
      {
        kind: 'TestPointKeyword',
        lexeme: '    ok 1 - this should pass',
        nesting: 1,
        node: {
          description: 'this should pass',
          diagnostics: ['duration_ms: 0.0001'],
          id: '1',
          reason: '',
          status: {
            fail: false,
            pass: true,
            skip: false,
            todo: false,
          },
          time: 0.0001,
        },
      },
      {
        kind: 'PlanKeyword',
        lexeme: '    1..1',
        nesting: 1,
        node: {
          end: '1',
          start: '1',
        },
      },
    ],
  },
  {
    input: `TAP version 13
# Subtest: test 1
ok 1 - test 1
  ---
  foo: bar
  duration_ms: 1.00
  prop: |-
    multiple
    lines
  ...
# Subtest: test 2
ok 2 - test 2
  ---
  duration_ms: 2.00
  ...
# Subtest: test 3
ok 3 - test 3
  ---
  foo: bar
  duration_ms: 3.00
  prop: |-
    multiple
    lines
  ...`,
    expected: [
      {
        nesting: 0,
        kind: 'VersionKeyword',
        node: { version: '13' },
        lexeme: 'TAP version 13',
      },
      {
        nesting: 0,
        kind: 'SubTestPointKeyword',
        node: { name: 'test 1' },
        lexeme: '# Subtest: test 1',
      },
      {
        nesting: 0,
        kind: 'TestPointKeyword',
        node: {
          status: { fail: false, pass: true, todo: false, skip: false },
          id: '1',
          description: 'test 1',
          reason: '',
          time: 1.0,
          diagnostics: [
            'foo: bar',
            'duration_ms: 1.00',
            'prop: |-',
            '  multiple',
            '  lines',
          ],
        },
        lexeme: 'ok 1 - test 1',
      },
      {
        nesting: 0,
        kind: 'SubTestPointKeyword',
        node: { name: 'test 2' },
        lexeme: '# Subtest: test 2',
      },
      {
        nesting: 0,
        kind: 'TestPointKeyword',
        node: {
          status: { fail: false, pass: true, todo: false, skip: false },
          id: '2',
          description: 'test 2',
          reason: '',
          time: 2.0,
          diagnostics: ['duration_ms: 2.00'],
        },
        lexeme: 'ok 2 - test 2',
      },
      {
        nesting: 0,
        kind: 'SubTestPointKeyword',
        node: { name: 'test 3' },
        lexeme: '# Subtest: test 3',
      },
      {
        nesting: 0,
        kind: 'TestPointKeyword',
        node: {
          status: { fail: false, pass: true, todo: false, skip: false },
          id: '3',
          description: 'test 3',
          reason: '',
          time: 3.0,
          diagnostics: [
            'foo: bar',
            'duration_ms: 3.00',
            'prop: |-',
            '  multiple',
            '  lines',
          ],
        },
        lexeme: 'ok 3 - test 3',
      },
    ],
  },
  {
    input: `TAP version 13
# Subtest: test 1
ok 1 - test 1
  ---
  foo: bar
  duration_ms: 1.00
  prop: |-
    multiple
    lines
  ...
    # Subtest: test 11
    ok 11 - test 11
      ---
      duration_ms: 11.00
      ...
        # Subtest: test 111
        ok 111 - test 111
          ---
          foo: bar
          duration_ms: 111.00
          prop: |-
            multiple
            lines
          ...`,
    expected: [
      {
        nesting: 0,
        kind: 'VersionKeyword',
        node: { version: '13' },
        lexeme: 'TAP version 13',
      },
      {
        nesting: 0,
        kind: 'SubTestPointKeyword',
        node: { name: 'test 1' },
        lexeme: '# Subtest: test 1',
      },
      {
        nesting: 0,
        kind: 'TestPointKeyword',
        node: {
          status: { fail: false, pass: true, todo: false, skip: false },
          id: '1',
          description: 'test 1',
          reason: '',
          time: 1.0,
          diagnostics: [
            'foo: bar',
            'duration_ms: 1.00',
            'prop: |-',
            '  multiple',
            '  lines',
          ],
        },
        lexeme: 'ok 1 - test 1',
      },
      {
        nesting: 1,
        kind: 'SubTestPointKeyword',
        node: { name: 'test 11' },
        lexeme: '    # Subtest: test 11',
      },
      {
        nesting: 1,
        kind: 'TestPointKeyword',
        node: {
          status: { fail: false, pass: true, todo: false, skip: false },
          id: '11',
          description: 'test 11',
          reason: '',
          time: 11.0,
          diagnostics: ['duration_ms: 11.00'],
        },
        lexeme: '    ok 11 - test 11',
      },
      {
        nesting: 2,
        kind: 'SubTestPointKeyword',
        node: { name: 'test 111' },
        lexeme: '        # Subtest: test 111',
      },
      {
        nesting: 2,
        kind: 'TestPointKeyword',
        node: {
          status: { fail: false, pass: true, todo: false, skip: false },
          id: '111',
          description: 'test 111',
          reason: '',
          time: 111.0,
          diagnostics: [
            'foo: bar',
            'duration_ms: 111.00',
            'prop: |-',
            '  multiple',
            '  lines',
          ],
        },
        lexeme: '        ok 111 - test 111',
      },
    ],
  },
];

(async () => {
  for (const { input, expected } of cases) {
    const parser = new TapParser();
    parser.write(input);
    parser.end();
    const actual = await parser.toArray();
    assert.deepStrictEqual(
      actual,
      expected.map((item) => ({ __proto__: null, ...item }))
    );
  }
})().then(common.mustCall());

(async () => {
  const expected = [
    {
      kind: 'PlanKeyword',
      node: { start: '1', end: '3' },
      nesting: 0,
      lexeme: '1..3',
    },
    {
      nesting: 0,
      kind: 'TestPointKeyword',
      node: {
        status: { fail: false, pass: true, todo: false, skip: false },
        id: '1',
        description: 'Input file opened',
        reason: '',
        time: 0,
        diagnostics: [],
      },
      lexeme: 'ok 1 - Input file opened',
    },
    {
      kind: 'TestPointKeyword',
      node: {
        status: { fail: true, pass: false, todo: false, skip: false },
        id: '2',
        description: '',
        reason: '',
        time: 0,
        diagnostics: [],
      },
      nesting: 0,
      lexeme: 'not ok    2 ',
    },
    {
      kind: 'SubTestPointKeyword',
      node: { name: 'foobar' },
      nesting: 1,
      lexeme: '    # Subtest: foobar',
    },
    {
      __proto__: null,
      kind: 'TestPointKeyword',
      node: {
        status: { fail: false, pass: true, todo: true, skip: false },
        id: '3',
        description: '',
        reason: '',
        time: 0.0001,
        diagnostics: [
          'foo: bar',
          'duration_ms:   0.0001',
          'prop: |-',
          '  foo',
          '  bar',
        ],
      },
      nesting: 0,
      lexeme: 'ok    3 # TODO',
    },
  ];

  const parser = new TapParser({ specs: TapChecker.TAP14 });
  parser.write('\n');
  parser.write('1');
  parser.write('.');
  parser.write('.');
  parser.write('3');
  parser.write('\n');
  parser.write('ok 1 ');
  parser.write('- Input file opened\n');
  parser.write('not');
  parser.write(' ok');
  parser.write('    2 \n');
  parser.write('\n');
  parser.write('    # ');
  parser.write('Subtest: foo');
  parser.write('bar');
  parser.write('\n');
  parser.write('');
  parser.write('ok');
  parser.write('    3 #');
  parser.write(' TODO');
  parser.write('\n');
  parser.write('  ---\n');
  parser.write('  foo: bar\n');
  parser.write('  duration_ms: ');
  parser.write('  0.0001\n');
  parser.write('  prop: |-\n');
  parser.write('    foo\n');
  parser.write('    bar\n');
  parser.write('  ...\n');
  parser.end();
  const actual = await parser.toArray();
  assert.deepStrictEqual(
    actual,
    expected.map((item) => ({ __proto__: null, ...item }))
  );
})().then(common.mustCall());
