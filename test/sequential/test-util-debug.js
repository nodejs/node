'use strict';
const common = require('../common');
var assert = require('assert');

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
  var expectErr = '';
  if (shouldWrite) {
    expectErr = 'TUD %PID%: this { is: \'a\' } /debugging/\n' +
                'TUD %PID%: number=1234 string=asdf obj={"foo":"bar"}\n';
  }
  var expectOut = 'ok\n';

  var spawn = require('child_process').spawn;
  var child = spawn(process.execPath, [__filename, 'child'], {
    env: Object.assign(process.env, { NODE_DEBUG: environ })
  });

  expectErr = expectErr.split('%PID%').join(child.pid);

  var err = '';
  child.stderr.setEncoding('utf8');
  child.stderr.on('data', function(c) {
    err += c;
  });

  var out = '';
  child.stdout.setEncoding('utf8');
  child.stdout.on('data', function(c) {
    out += c;
  });

  child.on('close', common.mustCall(function(c) {
    assert(!c);
    assert.equal(err, expectErr);
    assert.equal(out, expectOut);
    console.log('ok %j %j', environ, shouldWrite);
  }));
}


function child() {
  var util = require('util');
  var debug = util.debuglog('tud');
  debug('this', { is: 'a' }, /debugging/);
  debug('number=%d string=%s obj=%j', 1234, 'asdf', { foo: 'bar' });
  console.log('ok');
}
