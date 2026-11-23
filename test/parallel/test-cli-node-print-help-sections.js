// Flags: --expose-internals
'use strict';

// Tests that `node --help` groups options into the sections that
// node_options.cc assigns them to.

require('../common');
const assert = require('assert');
const { spawnSync } = require('child_process');
const { internalBinding } = require('internal/test/binding');
const { getCLIOptionsInfo } = require('internal/options');

const {
  helpSections,
  getNamespaceOptionsInputType,
} = internalBinding('options');

assert.ok(Array.isArray(helpSections));
assert.ok(helpSections.length > 0);
// print_help.js puts `-` and `--` in section 0.
assert.strictEqual(helpSections[0], 'General');
assert.strictEqual(new Set(helpSections).size, helpSections.length);

const { options } = getCLIOptionsInfo();
for (const [name, { helpSection }] of options) {
  assert.ok(Number.isInteger(helpSection) &&
            helpSection >= 0 && helpSection < helpSections.length,
            `${name} has invalid help section ${helpSection}`);
}

// Options in a config file namespace belong in that namespace's section.
const namespaceSections = {
  bench: 'Benchmark runner',
  permission: 'Permissions',
  test: 'Test runner',
  watch: 'Watch mode',
};
for (const [namespace, nsOptions] of getNamespaceOptionsInputType()) {
  const expected = namespaceSections[namespace];
  assert.ok(expected, `no expected help section for namespace ${namespace}`);
  for (const name of Object.keys(nsOptions)) {
    assert.strictEqual(helpSections[options.get(name).helpSection], expected,
                       `${name} is in namespace ${namespace}`);
  }
}

// Every non-empty section is printed once, in order, and each documented
// option is listed under its own section.
const { stdout, status } = spawnSync(process.execPath, ['--help'], {
  env: { ...process.env, NO_COLOR: '1', FORCE_COLOR: undefined },
  encoding: 'utf8',
});
assert.strictEqual(status, 0);

const printed = new Map();  // Section title -> text of that section.
let current;
for (const line of stdout.split('\n')) {
  if (line === 'Environment variables:') break;
  const heading = /^(\S.*):$/.exec(line);
  if (heading && helpSections.includes(heading[1])) {
    assert.ok(!printed.has(heading[1]), `${heading[1]} printed twice`);
    current = heading[1];
    printed.set(current, '');
  } else if (current !== undefined) {
    printed.set(current, printed.get(current) + line + '\n');
  }
}
assert.deepStrictEqual([...printed.keys()],
                       helpSections.filter((title) => printed.has(title)));

for (const [name, { helpText, helpSection, defaultIsTrue }] of options) {
  if (!helpText) continue;
  const title = helpSections[helpSection];
  const shown = defaultIsTrue ? `--no-${name.slice(2)}` : name;
  // Names are printed at the start of a line, possibly after aliases
  // (e.g. `-e, --eval=...`).
  const listed = new RegExp(`^  (?:\\S+, )*${RegExp.escape(shown)}(?:[=\\s\\[]|$)`, 'm');
  assert.match(printed.get(title) ?? '', listed,
               `${shown} not listed under ${title}`);
}
