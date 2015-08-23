'use strict';
const common = require('../common');
const assert = require('assert');

if (process.argv[2] !== 'child') {
  const spawn = require('child_process').spawn;
  const child = spawn(process.execPath, [__filename, 'child'], {
    stdio: 'pipe'//'inherit'
  });
  const timer = setTimeout(function() {
    throw new Error('child is hung');
  }, common.platformTimeout(3000));
  child.on('exit', function(code) {
    console.error('ok');
    assert(!code);
    clearTimeout(timer);
  });
} else {

  const domain = require('domain');
  const d = domain.create();
  process.maxTickDepth = 10;

  // in the error handler, we trigger several MakeCallback events
  d.on('error', function(e) {
    console.log('a');
    console.log('b');
    console.log('c');
    console.log('d');
    console.log('e');
    f();
  });

  function f() {
    process.nextTick(function() {
      d.run(function() {
        throw new Error('x');
      });
    });
  }

  f();
  setTimeout(function() {
    console.error('broke in!');
    //process.stdout.close();
    //process.stderr.close();
    process.exit(0);
  });
}
