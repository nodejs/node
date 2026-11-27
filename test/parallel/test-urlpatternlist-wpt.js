// Flags: --disable-warning=ExperimentalWarning
'use strict';

// URLPatternList is specified in terms of URLPattern: exec() returns the
// URLPattern#exec() result of the first pattern in the list that matches, and
// test() returns whether any pattern matches. This test checks that against
// every case of the URLPattern web-platform-tests.

require('../common');
const assert = require('assert');
const fixtures = require('../common/fixtures');
const fs = require('fs');
const { URLPattern, URLPatternList } = require('url');

const entries = JSON.parse(fs.readFileSync(
  fixtures.path('wpt', 'urlpattern', 'resources', 'urlpatterntestdata.json'),
  'utf8'));

function normalize(result) {
  if (result === null) return null;
  const normalized = {};
  for (const key of Object.keys(result)) {
    const value = result[key];
    if (key === 'pattern' || key === 'inputs') {
      normalized[key] = value;
    } else {
      normalized[key] = { input: value.input, groups: { ...value.groups } };
    }
  }
  return normalized;
}

function patternExec(pattern, inputs) {
  const result = pattern.exec(...inputs);
  return result === null ? null : normalize({ ...result, pattern });
}

const patterns = [];
const cases = [];
for (const entry of entries) {
  if (entry.expected_obj === 'error') continue;
  const pattern = new URLPattern(...entry.pattern);
  patterns.push(pattern);
  if (entry.inputs !== undefined) {
    cases.push({ pattern, inputs: entry.inputs,
                 constructorArgs: entry.pattern,
                 throws: entry.expected_match === 'error' });
  }
}
assert.ok(cases.length > 250);

// A list of one pattern behaves like the pattern.
for (const { pattern, inputs, constructorArgs, throws } of cases) {
  const list = new URLPatternList([pattern]);
  if (throws) {
    assert.throws(() => pattern.exec(...inputs), TypeError);
    assert.throws(() => list.exec(...inputs), TypeError);
    assert.throws(() => list.test(...inputs), TypeError);
    continue;
  }
  const message =
    `new URLPattern(...${JSON.stringify(constructorArgs)}) ` +
    `with ${JSON.stringify(inputs)}`;
  assert.deepStrictEqual(normalize(list.exec(...inputs)),
                         patternExec(pattern, inputs), message);
  assert.strictEqual(list.test(...inputs), pattern.test(...inputs), message);
}

// A list of every pattern finds the first one, in list order, that matches.
// The patterns have special and non-special schemes, and some of them ignore
// case, so the list mixes patterns that its pathname index covers and
// patterns that it does not.
for (const list of [patterns, patterns.toReversed()]) {
  const patternList = new URLPatternList(list);
  for (const { inputs, throws } of cases) {
    if (throws) {
      assert.throws(() => patternList.exec(...inputs), TypeError);
      assert.throws(() => patternList.test(...inputs), TypeError);
      continue;
    }
    const first = list.find((pattern) => pattern.test(...inputs));
    const message = JSON.stringify(inputs);
    assert.deepStrictEqual(normalize(patternList.exec(...inputs)),
                           first ? patternExec(first, inputs) : null,
                           message);
    assert.strictEqual(patternList.test(...inputs), first !== undefined,
                       message);
  }
}
