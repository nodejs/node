'use strict';
const common = require('../common');
const assert = require('assert');
const repl = require('repl');

test();

function test() {
  testParseREPLKeyword();
}

function testParseREPLKeyword() {
  const server = repl.start({ prompt: '> ' });
  const warn = 'REPLServer.parseREPLKeyword() is deprecated';

  common.expectWarning('DeprecationWarning', warn);
  assert.ok(server.parseREPLKeyword('clear'));
  common.expectWarning('DeprecationWarning', warn);
  assert.ok(!server.parseREPLKeyword('tacos'));
  server.close();
}
