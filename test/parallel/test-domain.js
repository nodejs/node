'use strict';
// Simple tests of most basic domain functionality.

require('../common');
const assert = require('assert');
const domain = require('domain');
const events = require('events');
const fs = require('fs');
var caught = 0;
var expectCaught = 0;

var d = new domain.Domain();
var e = new events.EventEmitter();

d.on('error', function(er) {
  console.error('caught', er && (er.message || er));

  var er_message = er.message;
  var er_path = er.path;

  // On windows, error messages can contain full path names. If this is the
  // case, remove the directory part.
  if (typeof er_path === 'string') {
    var slash = er_path.lastIndexOf('\\');
    if (slash !== -1) {
      var dir = er_path.slice(0, slash + 1);
      er_path = er_path.replace(dir, '');
      er_message = er_message.replace(dir, '');
    }
  }

  switch (er_message) {
    case 'emitted':
      assert.strictEqual(er.domain, d);
      assert.strictEqual(er.domainEmitter, e);
      assert.strictEqual(er.domainThrown, false);
      break;

    case 'bound':
      assert.ok(!er.domainEmitter);
      assert.strictEqual(er.domain, d);
      assert.strictEqual(er.domainBound, fn);
      assert.strictEqual(er.domainThrown, false);
      break;

    case 'thrown':
      assert.ok(!er.domainEmitter);
      assert.strictEqual(er.domain, d);
      assert.strictEqual(er.domainThrown, true);
      break;

    case "ENOENT: no such file or directory, open 'this file does not exist'":
      assert.strictEqual(er.domain, d);
      assert.strictEqual(er.domainThrown, false);
      assert.strictEqual(typeof er.domainBound, 'function');
      assert.ok(!er.domainEmitter);
      assert.strictEqual(er.code, 'ENOENT');
      assert.strictEqual(er_path, 'this file does not exist');
      assert.strictEqual(typeof er.errno, 'number');
      break;

    case
    "ENOENT: no such file or directory, open 'stream for nonexistent file'":
      assert.strictEqual(typeof er.errno, 'number');
      assert.strictEqual(er.code, 'ENOENT');
      assert.strictEqual(er_path, 'stream for nonexistent file');
      assert.strictEqual(er.domain, d);
      assert.strictEqual(er.domainEmitter, fst);
      assert.ok(!er.domainBound);
      assert.strictEqual(er.domainThrown, false);
      break;

    case 'implicit':
      assert.strictEqual(er.domainEmitter, implicit);
      assert.strictEqual(er.domain, d);
      assert.strictEqual(er.domainThrown, false);
      assert.ok(!er.domainBound);
      break;

    case 'implicit timer':
      assert.strictEqual(er.domain, d);
      assert.strictEqual(er.domainThrown, true);
      assert.ok(!er.domainEmitter);
      assert.ok(!er.domainBound);
      break;

    case 'Cannot read property \'isDirectory\' of undefined':
      assert.strictEqual(er.domain, d);
      assert.ok(!er.domainEmitter);
      assert.ok(!er.domainBound);
      break;

    case 'nextTick execution loop':
      assert.strictEqual(er.domain, d);
      assert.ok(!er.domainEmitter);
      assert.ok(!er.domainBound);
      break;

    default:
      console.error('unexpected error, throwing %j', er.message, er);
      throw er;
  }

  caught++;
});


process.on('exit', function() {
  console.error('exit', caught, expectCaught);
  assert.strictEqual(caught, expectCaught,
                     'caught the expected number of errors');
  console.log('ok');
});


// revert to using the domain when a callback is passed to nextTick in
// the middle of a tickCallback loop
d.run(function() {
  process.nextTick(function() {
    throw new Error('nextTick execution loop');
  });
});
expectCaught++;


// catch thrown errors no matter how many times we enter the event loop
// this only uses implicit binding, except for the first function
// passed to d.run().  The rest are implicitly bound by virtue of being
// set up while in the scope of the d domain.
d.run(function() {
  process.nextTick(function() {
    var i = setInterval(function() {
      clearInterval(i);
      setTimeout(function() {
        fs.stat('this file does not exist', function(er, stat) {
          // uh oh!  stat isn't set!
          // pretty common error.
          console.log(stat.isDirectory());
        });
      }, 1);
    }, 1);
  });
});
expectCaught++;


// implicit addition of a timer created within a domain-bound context.
d.run(function() {
  setTimeout(function() {
    throw new Error('implicit timer');
  }, 1);
});
expectCaught++;


// Event emitters added to the domain have their errors routed.
d.add(e);
e.emit('error', new Error('emitted'));
expectCaught++;


// get rid of the `if (er) return cb(er)` malarky, by intercepting
// the cb functions to the domain, and using the intercepted function
// as a callback instead.
function fn() {
  throw new Error('This function should never be called!');
}

var bound = d.intercept(fn);
bound(new Error('bound'));
expectCaught++;


// intercepted should never pass first argument to callback
function fn2(data) {
  assert.strictEqual(data, 'data', 'should not be null err argument');
}

bound = d.intercept(fn2);
bound(null, 'data');

// intercepted should never pass first argument to callback
// even if arguments length is more than 2.
function fn3(data, data2) {
  assert.strictEqual(data, 'data', 'should not be null err argument');
  assert.strictEqual(data2, 'data2', 'should not be data argument');
}

bound = d.intercept(fn3);
bound(null, 'data', 'data2');

// throwing in a bound fn is also caught,
// even if it's asynchronous, by hitting the
// global uncaughtException handler. This doesn't
// require interception, since throws are always
// caught by the domain.
function thrower() {
  throw new Error('thrown');
}
setTimeout(d.bind(thrower), 100);
expectCaught++;


// Pass an intercepted function to an fs operation that fails.
fs.open('this file does not exist', 'r', d.intercept(function(er) {
  console.error('should not get here!', er);
  throw new Error('should not get here!');
}, true));
expectCaught++;


// implicit addition by being created within a domain-bound context.
var implicit;

d.run(function() {
  implicit = new events.EventEmitter();
});

setTimeout(function() {
  // escape from the domain, but implicit is still bound to it.
  implicit.emit('error', new Error('implicit'));
}, 10);
expectCaught++;


var result = d.run(function() {
  return 'return value';
});
assert.strictEqual(result, 'return value');


// check if the executed function take in count the applied parameters
result = d.run(function(a, b) {
  return a + ' ' + b;
}, 'return', 'value');
assert.strictEqual(result, 'return value');


var fst = fs.createReadStream('stream for nonexistent file');
d.add(fst);
expectCaught++;

[42, null, , false, function() {}, 'string'].forEach(function(something) {
  var d = new domain.Domain();
  d.run(function() {
    process.nextTick(function() {
      throw something;
    });
    expectCaught++;
  });

  d.on('error', function(er) {
    assert.strictEqual(something, er);
    caught++;
  });
});
