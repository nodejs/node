// Flags: --expose-internals
'use strict';
const common = require('../common');
const assert = require('node:assert');
const { TapParser } = require('internal/test_runner/tap_parser');

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
          diagnostics: ['foo: bar', 'duration_ms: 0.0001', 'prop: |-', '  multiple', '  lines'],
        },
        lexeme: 'ok 1 - test',
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
