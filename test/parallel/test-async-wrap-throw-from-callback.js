'use strict';

require('../common');
const async_wrap = process.binding('async_wrap');
const assert = require('assert');
const crypto = require('crypto');
const domain = require('domain');
const spawn = require('child_process').spawn;
const callbacks = [ 'init', 'pre', 'post' ];
const toCall = process.argv[2];
let msgCalled = 0;
let msgReceived = 0;

function init() {
  if (toCall === 'init')
    throw new Error('init');
}
function pre() {
  if (toCall === 'pre')
    throw new Error('pre');
}
function post() {
  if (toCall === 'post')
    throw new Error('post');
}

if (typeof process.argv[2] === 'string') {
  async_wrap.setupHooks({ init, pre, post });
  async_wrap.enable();

  process.on('uncaughtException', () => assert.ok(0, 'UNREACHABLE'));

  const d = domain.create();
  d.on('error', () => assert.ok(0, 'UNREACHABLE'));
  d.run(() => {
    // Using randomBytes because timers are not yet supported.
    crypto.randomBytes(0, () => { });
  });

} else {

  process.on('exit', (code) => {
    assert.strictEqual(msgCalled, callbacks.length);
    assert.strictEqual(msgCalled, msgReceived);
  });

  callbacks.forEach((item) => {
    msgCalled++;

    const child = spawn(process.execPath, [__filename, item]);
    let errstring = '';

    child.stderr.on('data', (data) => {
      errstring += data.toString();
    });

    child.on('close', (code) => {
      if (errstring.includes('Error: ' + item))
        msgReceived++;

      assert.strictEqual(code, 1, `${item} closed with code ${code}`);
    });
  });
}
