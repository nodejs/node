// Flags: --expose-internals
'use strict';
require('../common');
const assert = require('assert');
const { spawnSync } = require('child_process');

const jsonPath = '/package-json-parse/package.json';

function fields({ name = null, main = null, type = 'none', imports = null,
                  exports = null, isPrivate = null } = {}) {
  return { value: [name, main, type, imports, exports, jsonPath, isPrivate] };
}

const invalid = { error: 'ERR_INVALID_PACKAGE_CONFIG' };

const cases = [
  ['{"main": "index.js"}', fields({ main: 'index.js' })],
  ['﻿{"name": "bom", "type": "module"}',
   fields({ name: 'bom', type: 'module' })],
  ['{"name": "esc\\"aped\\u00e9\\n", "main": 42}',
   fields({ name: 'esc"apedé\n' })],
  ['{"exports": {"./a": {"import": "./a.mjs"}}, "imports": ["#x"]}',
   fields({ exports: '{"./a": {"import": "./a.mjs"}}', imports: '["#x"]' })],
  ['{"exports": "./index.js", "imports": "./i.js"}',
   fields({ exports: './index.js', imports: './i.js' })],
  ['{"exports": null, "imports": 7, "scripts": ["not", "object"]}', fields()],
  ['{"scripts": {"test": "node --test", "build": "tsc"}}', fields()],
  ['{"type": "commonjs"}', fields({ type: 'commonjs' })],
  ['{"type": "banana"}', fields()],
  ['{"private": true}', fields({ isPrivate: true })],
  ['{"private": "yes"}', fields()],
  ['{"name": "a", "name": "b", "type": "module", "type": "commonjs"}',
   fields({ name: 'b', type: 'commonjs' })],
  ['{"type": "module", "type": "banana", "main": "a", "main": 7}',
   fields({ type: 'module', main: 'a' })],
  ['{"\\u006e\\u0061\\u006d\\u0065": "fully-escaped-key", "type": "module"}',
   fields({ type: 'module' })],
  ['{"na\\u006de": "escaped-key"}', fields()],
  ['{"name": "foo", "exports": "escaped \\" \\\\/ \\b \\f \\n \\r \\t \\\\ ' +
   '\\u0020 bar", "type": "module"}',
   fields({
     name: 'foo',
     exports: JSON.parse('"escaped \\" \\\\/ \\b \\f \\n \\r \\t \\\\ \\u0020 bar"'),
     type: 'module',
   })],
  ['{"exports": {"./foo\\"bar": "./baz.js", "./q": ["\\u0041", {"x\\\\": "y"}]}, ' +
   '"imports": {"#a\\"b": "./c.js"}}',
   fields({
     exports: '{"./foo\\"bar": "./baz.js", "./q": ["\\u0041", {"x\\\\": "y"}]}',
     imports: '{"#a\\"b": "./c.js"}',
   })],
  ['{"exports": {"deep": {"deeper": [1, {"x": "y"}]}}, "main": "after.js"}',
   fields({ exports: '{"deep": {"deeper": [1, {"x": "y"}]}}', main: 'after.js' })],
  ['{"name": "foo", "type": "module"}   \n\t ',
   fields({ name: 'foo', type: 'module' })],
  ['  {"main" : "spaced.js" ,  "type" :"module"}  ',
   fields({ main: 'spaced.js', type: 'module' })],
  ['{}', fields()],
  ['{"name": "foo"} {"name": "bar"}', fields({ name: 'foo' })],
  ['{"name": "foo"}}', fields({ name: 'foo' })],
  ['{"name": "foo"} x}', fields({ name: 'foo' })],
  ['{"name": "foo", "bogus": [1, 2,], "type": "module"}',
   fields({ name: 'foo', type: 'module' })],
  ['{"name": "foo", "bogus": tru, "type": "module"}',
   fields({ name: 'foo', type: 'module' })],
  ['{"name": "foo", "bogus": {"a" 1}, "type": "module"}',
   fields({ name: 'foo', type: 'module' })],
  ['{"name": "foo"} trailing junk', invalid],
  ['{"name": "foo"},', invalid],
  ['{"name": "foo"} ]', invalid],
  ['{"name": "foo"} 1', invalid],
  ['{"name": "foo"} "x"', invalid],
  ['{"name": "foo"} true', invalid],
  ['{"name": "foo"}x', invalid],
  ['{"name": 1}', invalid],
  ['{"type": 1}', invalid],
  ['{"main": "x"', invalid],
  ['{"main": "x",}', invalid],
  ['[1, 2, 3]', invalid],
  ['"just a string"', invalid],
  ['', invalid],
];

for (let target = 250; target <= 1100; target += 13) {
  const filler = 'x'.repeat(target % 61);
  const pkg = {
    name: `pkg-${target}-\\"esc\\"-${filler}`,
    exports: { '.': { import: `./${filler}.mjs` }, './sub': [`./a-${target}.js`] },
    main: `./${'m'.repeat(target % 37)}.js`,
  };
  let text = JSON.stringify(pkg);
  while (text.length < target) {
    pkg[`k${text.length}`] = 'v'.repeat(target - text.length);
    text = JSON.stringify(pkg);
  }
  const expected = {
    name: pkg.name,
    main: pkg.main,
    exports: JSON.stringify(pkg.exports),
  };
  cases.push(
    [text, fields(expected)],
    [`${text.slice(0, -1)}, "type": "module"}`, fields({ ...expected, type: 'module' })],
    [text.slice(0, -1), invalid],
  );
}

if (process.argv[2] === 'child') {
  const { internalBinding } = require('internal/test/binding');
  const { parsePackageJSON } = internalBinding('modules');
  const results = cases.map(([input]) => {
    try {
      return { value: parsePackageJSON(Buffer.from(input), jsonPath) };
    } catch (error) {
      return { error: error.code };
    }
  });
  process.stdout.write(JSON.stringify(results, (key, value) => value ?? null));
  return;
}

for (const isa of [undefined, 'avx512', 'avx2', 'fallback']) {
  const env = { ...process.env };
  delete env.NODE_JSONIFIER_ISA;
  if (isa !== undefined) env.NODE_JSONIFIER_ISA = isa;
  const child = spawnSync(process.execPath, ['--expose-internals', __filename, 'child'], {
    env,
    encoding: 'utf8',
  });
  assert.strictEqual(child.status, 0, child.stderr);
  const results = JSON.parse(child.stdout);
  for (let i = 0; i < cases.length; i++) {
    assert.deepStrictEqual(results[i], cases[i][1],
                           `isa ${isa ?? 'auto'}, input ${i}: ${JSON.stringify(cases[i][0])}`);
  }
}
