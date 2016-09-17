'use strict';
const common = require('../common');
const assert = require('assert');
const repl = require('repl');

common.expectWarning('DeprecationWarning',
                     'replServer.convertToContext() is deprecated');

// Create a dummy stream that does nothing
const stream = new common.ArrayStream();

const replServer = repl.start({
  input: stream,
  output: stream
});

const cmd = replServer.convertToContext('var name = "nodejs"');
assert.strictEqual(cmd, 'self.context.name = "nodejs"');
