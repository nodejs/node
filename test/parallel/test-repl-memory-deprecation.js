'use strict';
const common = require('../common');
const assert = require('assert');
const repl = require('repl');

testMemory();

function testMemory() {
  const server = repl.start({ prompt: '> ' });
  const warn = 'REPLServer.memory() is deprecated';

  common.expectWarning('DeprecationWarning', warn, 'DEP0082');
  assert.strictEqual(server.memory(), undefined);
  server.close();
}
