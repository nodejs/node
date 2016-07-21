'use strict';
const common = require('../common');
const assert = require('assert');
const repl = require('repl');

const expected = [
  'replServer.convertToContext() is deprecated'
];

process.on('warning', common.mustCall((warning) => {
  assert.strictEqual(warning.name, 'DeprecationWarning');
  assert.notStrictEqual(expected.indexOf(warning.message), -1,
                        `unexpected error message: "${warning.message}"`);
  // Remove a warning message after it is seen so that we guarantee that we get
  // each message only once.
  expected.splice(expected.indexOf(warning.message), 1);
}, expected.length));

// Create a dummy stream that does nothing
const stream = new common.ArrayStream();

const replServer = repl.start({
  input: stream,
  output: stream
});

const cmd = replServer.convertToContext('var name = "nodejs"');
assert.strictEqual(cmd, 'self.context.name = "nodejs"');
