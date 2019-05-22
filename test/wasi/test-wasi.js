'use strict';

require('../common');

const cp = require('child_process');
const fs = require('fs');
const path = require('path');
const assert = require('assert');

const SKIP = [
  'poll',
];

const C_DIR = path.resolve(__dirname, 'c');
const WASM_DIR = path.resolve(__dirname, 'wasm');

// TODO(devsnek): remove this when V8 supports
// bigint<->i64 on non-x64 platforms.
if (process.arch !== 'x64') {
  console.log('Skipping WASI tests on non-x64 platform');
  return;
}

function parseOptions(string) {
  const options = {};
  for (const line of string.split('\n')) {
    if (!line.startsWith('//')) {
      break;
    }
    const [key, value] = line.slice(3).split(': ');
    if (!key || !value) {
      continue;
    }
    options[key] = value.replace(/\\n/g, '\n');
  }
  return options;
}

function exec(command, { stdin } = {}) {
  try {
    const stdout = cp.execSync(command, {
      input: stdin,
    });
    return { code: 0, stdout: stdout.toString() };
  } catch (e) {
    if (e.status) {
      return { code: e.status, stdout: '' };
    }
    throw e;
  }
}

const ARGS = [
  '--expose-internals',
  `-r ${__dirname}/patch_preopen`,
  '--experimental-wasm-bigint',
  '--experimental-modules',
  '--experimental-wasi-modules',
].join(' ');

fs.readdirSync(WASM_DIR).forEach((filename) => {
  const file = filename.split('.')[0];

  if (SKIP.includes(file)) {
    return;
  }

  const source = fs.readFileSync(path.join(C_DIR, `${file}.c`), 'utf8');
  const options = parseOptions(source);

  console.log('Running', file);

  const { code, stdout } =
    exec(`${process.execPath} ${ARGS} ${WASM_DIR}/${filename}`, {
      stdin: options.STDIN,
    });

  if (options.EXIT) {
    assert.strictEqual(code, +options.EXIT);
  } else {
    assert.strictEqual(code, 0);
  }

  assert.strictEqual(stdout, options.STDOUT || '');
});
