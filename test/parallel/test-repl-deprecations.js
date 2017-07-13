'use strict';
const common = require('../common');
const assert = require('assert');
const repl = require('repl');

testParseREPLKeyword();

function testParseREPLKeyword() {
  const server = repl.start({ prompt: '> ' });
  const warn = 'REPLServer.parseREPLKeyword() is deprecated';

  common.expectWarning('DeprecationWarning', warn);
  assert.ok(server.parseREPLKeyword('clear'));
  assert.ok(!server.parseREPLKeyword('tacos'));
  server.close();
}
