// Flags: --expose-internals
'use strict';
const common = require('../common');

common.skipIfInspectorDisabled();

const assert = require('assert');
const { NodeInstance } = require('../common/inspector-helper.js');
const os = require('os');

const ip = pickIPv4Address();

if (!ip)
  common.skip('No IP address found');

function checkIpAddress(ip, response) {
  const res = response[0];
  const wsUrl = res.webSocketDebuggerUrl;
  assert.ok(wsUrl);
  const match = wsUrl.match(/^ws:\/\/(.*):\d+\/(.*)/);
  assert.strictEqual(ip, match[1]);
  assert.strictEqual(res.id, match[2]);
  assert.strictEqual(ip, res.devtoolsFrontendUrl.match(/.*ws=(.*):\d+/)[1]);
}

function pickIPv4Address() {
  for (const i of [].concat(...Object.values(os.networkInterfaces()))) {
    if (i.family === 'IPv4' && i.address !== '127.0.0.1')
      return i.address;
  }
}

async function test() {
  const instance = new NodeInstance('--inspect-brk=0.0.0.0:0');
  try {
    checkIpAddress(ip, await instance.httpGet(ip, '/json/list'));
  } catch (error) {
    if (error.code === 'EHOSTUNREACH') {
      common.printSkipMessage('Unable to connect to self');
    } else {
      throw error;
    }
  }
  instance.kill();
}

test();
