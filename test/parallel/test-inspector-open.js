'use strict';
const common = require('../common');
common.skipIfInspectorDisabled();

// Test inspector open()/close()/url() API. It uses ephemeral ports so can be
// run safely in parallel.

const assert = require('assert');
const fork = require('child_process').fork;
const net = require('net');
const url = require('url');

if (process.env.BE_CHILD)
  return beChild();

const child = fork(__filename, {env: {BE_CHILD: 1}});

child.once('message', common.mustCall((msg) => {
  assert.strictEqual(msg.cmd, 'started');

  child.send({cmd: 'open', args: [0]});
  child.once('message', common.mustCall(firstOpen));
}));

let firstPort;

function firstOpen(msg) {
  assert.strictEqual(msg.cmd, 'url');
  const port = url.parse(msg.url).port;
  ping(port, (err) => {
    assert.ifError(err);
    // Inspector is already open, and won't be reopened, so args don't matter.
    child.send({cmd: 'open', args: []});
    child.once('message', common.mustCall(tryToOpenWhenOpen));
    firstPort = port;
  });
}

function tryToOpenWhenOpen(msg) {
  assert.strictEqual(msg.cmd, 'url');
  const port = url.parse(msg.url).port;
  // Reopen didn't do anything, the port was already open, and has not changed.
  assert.strictEqual(port, firstPort);
  ping(port, (err) => {
    assert.ifError(err);
    child.send({cmd: 'close'});
    child.once('message', common.mustCall(closeWhenOpen));
  });
}

function closeWhenOpen(msg) {
  assert.strictEqual(msg.cmd, 'url');
  assert.strictEqual(msg.url, undefined);
  ping(firstPort, (err) => {
    assert(err);
    child.send({cmd: 'close'});
    child.once('message', common.mustCall(tryToCloseWhenClosed));
  });
}

function tryToCloseWhenClosed(msg) {
  assert.strictEqual(msg.cmd, 'url');
  assert.strictEqual(msg.url, undefined);
  child.send({cmd: 'open', args: []});
  child.once('message', common.mustCall(reopenAfterClose));
}

function reopenAfterClose(msg) {
  assert.strictEqual(msg.cmd, 'url');
  const port = url.parse(msg.url).port;
  ping(port, (err) => {
    assert.ifError(err);
    process.exit();
  });
}

function ping(port, callback) {
  net.connect(port)
    .on('connect', function() { close(this); })
    .on('error', function(err) { close(this, err); });

  function close(self, err) {
    self.end();
    self.on('close', () => callback(err));
  }
}

function beChild() {
  const inspector = require('inspector');

  process.send({cmd: 'started'});

  process.on('message', (msg) => {
    if (msg.cmd === 'open') {
      inspector.open(...msg.args);
    }
    if (msg.cmd === 'close') {
      inspector.close();
    }
    process.send({cmd: 'url', url: inspector.url()});
  });
}
