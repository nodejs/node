// Flags: --expose-internals

'use strict';
const common = require('../common');
const { internalBinding } = require('internal/test/binding');

// Monkey patch before requiring anything
class DummyParser {
  constructor() {
    this.test_type = null;
  }
  initialize(type) {
    this.test_type = type;
  }
}
DummyParser.REQUEST = Symbol();

const binding = internalBinding('http_parser');
binding.HTTPParser = DummyParser;

const assert = require('assert');
const { spawn } = require('child_process');
const { parsers } = require('_http_common');

// Test _http_common was not loaded before monkey patching
const parser = parsers.alloc();
parser.initialize(DummyParser.REQUEST, {});
assert.strictEqual(parser instanceof DummyParser, true);
assert.strictEqual(parser.test_type, DummyParser.REQUEST);

if (process.argv[2] !== 'child') {
  // Also test in a child process with IPC (specific case of https://github.com/nodejs/node/issues/23716)
  const child = spawn(process.execPath, [
    '--expose-internals', __filename, 'child'
  ], {
    stdio: ['inherit', 'inherit', 'inherit', 'ipc']
  });
  child.on('exit', common.mustCall((code, signal) => {
    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
  }));
}
