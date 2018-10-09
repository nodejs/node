// Flags: --expose-internals
'use strict';
const common = require('../common');

common.skipIfInspectorDisabled();

const assert = require('assert');
const { NodeInstance } = require('../common/inspector-helper.js');

async function test() {
  const madeUpHost = '111.111.111.111:11111';
  const child = new NodeInstance(undefined, 'var a = 1');
  const response = await child.httpGet(null, '/json', madeUpHost);
  assert.ok(
    response[0].webSocketDebuggerUrl.startsWith(`ws://${madeUpHost}`),
    response[0].webSocketDebuggerUrl);
  child.kill();
}

test();
