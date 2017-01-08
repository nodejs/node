'use strict';
const common = require('../common');
const assert = require('assert');

if (process.argv[2] === 'child')
  child();
else
  parent();

function parent() {
  test('foo,tud,bar', true);
  test('foo,tud', true);
  test('tud,bar', true);
  test('tud', true);
  test('foo,bar', false);
  test('', false);
}

function test(environ, shouldWrite) {
  let expectErr = '';
  if (shouldWrite) {
    expectErr = 'TUD %PID%: this { is: \'a\' } /debugging/\n' +
                'TUD %PID%: number=1234 string=asdf obj={"foo":"bar"}\n';
  }
  const expectOut = 'ok\n';

  const spawn = require('child_process').spawn;
  const child = spawn(process.execPath, [__filename, 'child'], {
    env: Object.assign(process.env, { NODE_DEBUG: environ })
  });

  expectErr = expectErr.split('%PID%').join(child.pid);

  let err = '';
  child.stderr.setEncoding('utf8');
  child.stderr.on('data', function(c) {
    err += c;
  });

  let out = '';
  child.stdout.setEncoding('utf8');
  child.stdout.on('data', function(c) {
    out += c;
  });

  child.on('close', common.mustCall(function(c) {
    assert(!c);
    assert.strictEqual(err, expectErr);
    assert.strictEqual(out, expectOut);
    console.log('ok %j %j', environ, shouldWrite);
  }));
}


function child() {
  const util = require('util');
  const debug = util.debuglog('tud');
  debug('this', { is: 'a' }, /debugging/);
  debug('number=%d string=%s obj=%j', 1234, 'asdf', { foo: 'bar' });
  console.log('ok');
}
