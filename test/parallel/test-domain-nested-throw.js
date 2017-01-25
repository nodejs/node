'use strict';
require('../common');
const assert = require('assert');

const domain = require('domain');

let dispose;
switch (process.argv[2]) {
  case 'true':
    dispose = true;
    break;
  case 'false':
    dispose = false;
    break;
  default:
    parent();
    return;
}

function parent() {
  const node = process.execPath;
  const spawn = require('child_process').spawn;
  const opt = { stdio: 'inherit' };
  let child = spawn(node, [__filename, 'true'], opt);
  child.on('exit', function(c) {
    assert(!c);
    child = spawn(node, [__filename, 'false'], opt);
    child.on('exit', function(c) {
      assert(!c);
      console.log('ok');
    });
  });
}

let gotDomain1Error = false;
let gotDomain2Error = false;

let threw1 = false;
let threw2 = false;

function throw1() {
  threw1 = true;
  throw new Error('handled by domain1');
}

function throw2() {
  threw2 = true;
  throw new Error('handled by domain2');
}

function inner(throw1, throw2) {
  const domain1 = domain.createDomain();

  domain1.on('error', function(err) {
    if (gotDomain1Error) {
      console.error('got domain 1 twice');
      process.exit(1);
    }
    if (dispose) domain1.dispose();
    gotDomain1Error = true;
    throw2();
  });

  domain1.run(function() {
    throw1();
  });
}

function outer() {
  const domain2 = domain.createDomain();

  domain2.on('error', function(err) {
    if (gotDomain2Error) {
      console.error('got domain 2 twice');
      process.exit(1);
    }
    gotDomain2Error = true;
  });

  domain2.run(function() {
    inner(throw1, throw2);
  });
}

process.on('exit', function() {
  assert(gotDomain1Error);
  assert(gotDomain2Error);
  assert(threw1);
  assert(threw2);
  console.log('ok', dispose);
});

outer();
