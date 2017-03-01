'use strict';
const common = require('../common');
common.skipIfInspectorDisabled();

const assert = require('assert');
const helper = require('./inspector-helper.js');
const os = require('os');

const ip = pickIPv4Address();

if (!ip) {
  common.skip('No IP address found');
  return;
}

function checkListResponse(instance, err, response) {
  assert.ifError(err);
  const res = response[0];
  const wsUrl = res['webSocketDebuggerUrl'];
  assert.ok(wsUrl);
  const match = wsUrl.match(/^ws:\/\/(.*):9229\/(.*)/);
  assert.strictEqual(ip, match[1]);
  assert.strictEqual(res['id'], match[2]);
  assert.strictEqual(ip, res['devtoolsFrontendUrl'].match(/.*ws=(.*):9229/)[1]);
  instance.childInstanceDone = true;
}

function checkError(instance, error) {
  // Some OSes will not allow us to connect
  if (error.code === 'EHOSTUNREACH') {
    common.skip('Unable to connect to self');
  } else {
    throw error;
  }
  instance.childInstanceDone = true;
}

function runTests(instance) {
  instance
    .testHttpResponse(ip, '/json/list', checkListResponse.bind(null, instance),
                      checkError.bind(null, instance))
    .kill();
}

function pickIPv4Address() {
  for (const i of [].concat(...Object.values(os.networkInterfaces()))) {
    if (i.family === 'IPv4' && i.address !== '127.0.0.1')
      return i.address;
  }
}

helper.startNodeForInspectorTest(runTests, '--inspect-brk=0.0.0.0');
