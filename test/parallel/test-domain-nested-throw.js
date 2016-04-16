'use strict';
require('../common');
var assert = require('assert');

var domain = require('domain');

var dispose;
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
  var node = process.execPath;
  var spawn = require('child_process').spawn;
  var opt = { stdio: 'inherit' };
  var child = spawn(node, [__filename, 'true'], opt);
  child.on('exit', function(c) {
    assert(!c);
    child = spawn(node, [__filename, 'false'], opt);
    child.on('exit', function(c) {
      assert(!c);
      console.log('ok');
    });
  });
}

var gotDomain1Error = false;
var gotDomain2Error = false;

var threw1 = false;
var threw2 = false;

function throw1() {
  threw1 = true;
  throw new Error('handled by domain1');
}

function throw2() {
  threw2 = true;
  throw new Error('handled by domain2');
}

function inner(throw1, throw2) {
  var domain1 = domain.createDomain();

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
  var domain2 = domain.createDomain();

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
