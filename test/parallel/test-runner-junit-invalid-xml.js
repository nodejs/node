'use strict';
// Regression tests for the junit reporter emitting malformed XML.
require('../common');
const fixtures = require('../common/fixtures');
const assert = require('node:assert');
const { spawnSync } = require('node:child_process');
const { test } = require('node:test');
const { junit } = require('node:test/reporters');

// Characters that XML 1.0 forbids anywhere, even as character references.
const kInvalidXMLChars = /[^\t\n\r\x20-퟿-�\u{10000}-\u{10FFFF}]/u;

function assertWellFormedChars(xml) {
  assert.doesNotMatch(xml, kInvalidXMLChars);
  for (const { 1: dec, 2: hex } of xml.matchAll(/&#(?:(\d+)|x([0-9a-fA-F]+));/g)) {
    const code = dec !== undefined ? Number(dec) : Number.parseInt(hex, 16);
    assert.doesNotMatch(String.fromCodePoint(code), kInvalidXMLChars,
                        `illegal character reference &#${dec ?? `x${hex}`};`);
  }
}

async function render(events) {
  let out = '';
  for await (const chunk of junit((async function*() { yield* events; })())) {
    out += chunk;
  }
  return out;
}

const details = { duration_ms: 1 };

test('control characters are not written to the report', async () => {
  const name = 'name\x00with\x1b[31mcontrols\x1b[39m\x07￾\uD800&#27;';
  const error = new Error('\x1b[32mexpected\x1b[39m\x0b\x0c\x00');
  const xml = await render([
    { type: 'test:start', data: { name, nesting: 0 } },
    { type: 'test:fail', data: { name, nesting: 0, details: { ...details, error } } },
    { type: 'test:diagnostic', data: { nesting: 0, message: 'diag\x00\x1b[1m' } },
  ]);
  assertWellFormedChars(xml);
  // ANSI escape sequences are stripped, other illegal characters are replaced.
  assert.match(xml, /name="name�withcontrols���&amp;#27;"/);
  assert.match(xml, /message="expected���"/);
  assert.match(xml, /<!-- diag� -->/);
});

test('colored assertion diffs produce a valid report', () => {
  const child = spawnSync(process.execPath, [
    '--test-reporter=junit',
    fixtures.path('test-runner', 'junit-colored-diff.js'),
  ], { env: { ...process.env, FORCE_COLOR: '1' } });
  const xml = child.stdout.toString();
  assert.strictEqual(child.status, 1, child.stderr.toString());
  assert.match(xml, /<failure /);
  assertWellFormedChars(xml);
});

test('a test that never reports a result does not swallow later tests', async () => {
  const xml = await render([
    { type: 'test:start', data: { name: 'suite', nesting: 0 } },
    { type: 'test:start', data: { name: 'first', nesting: 1 } },
    { type: 'test:pass', data: { name: 'first', nesting: 1, details } },
    { type: 'test:start', data: { name: 'next file', nesting: 0 } },
    { type: 'test:pass', data: { name: 'next file', nesting: 0, details } },
  ]);
  assert.doesNotMatch(xml, /undefined/);
  assert.strictEqual(xml, [
    '<?xml version="1.0" encoding="utf-8"?>',
    '<testsuites>',
    `\t<testsuite name="suite" disabled="0" errors="0" tests="1" failures="0" skipped="0" hostname="${require('os').hostname()}">`,
    '\t\t<testcase name="first" time="0.001000" classname="test"/>',
    '\t</testsuite>',
    '\t<testcase name="next file" time="0.001000" classname="test"/>',
    '</testsuites>',
    '',
  ].join('\n'));
});

test('an unfinished leaf test is reported as a failure', async () => {
  const xml = await render([
    { type: 'test:start', data: { name: 'lost', nesting: 0 } },
    { type: 'test:start', data: { name: 'ok', nesting: 0 } },
    { type: 'test:pass', data: { name: 'ok', nesting: 0, details } },
  ]);
  assert.match(xml, /<testcase name="lost" classname="test" failure="test did not report a result">\n\t\t<failure type="incomplete" message="test did not report a result"\/>\n\t<\/testcase>/);
  assert.match(xml, /<testcase name="ok" time="0\.001000" classname="test"\/>/);
});

test('results without a test:start are not wrapped in an undefined element', async () => {
  const xml = await render([
    { type: 'test:pass', data: { name: 'a', nesting: 0, details } },
    { type: 'test:pass', data: { name: 'b', nesting: 0, details } },
  ]);
  assert.doesNotMatch(xml, /undefined|name="root"/);
  assert.match(xml, /^\t<testcase name="a" [^\n]*\/>\n\t<testcase name="b" [^\n]*\/>$/m);
});
