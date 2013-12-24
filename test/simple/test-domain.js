// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.


// Simple tests of most basic domain functionality.

var common = require('../common');
var assert = require('assert');
var domain = require('domain');
var events = require('events');
var fs = require('fs');
var caught = 0;
var expectCaught = 0;

var d = new domain.Domain();
var e = new events.EventEmitter();

d.on('error', function(er) {
  console.error('caught', er && (er.message || er));

  var er_message = er.message;
  var er_path = er.path

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
      assert.equal(er.domain, d);
      assert.equal(er.domainEmitter, e);
      assert.equal(er.domainThrown, false);
      break;

    case 'bound':
      assert.ok(!er.domainEmitter);
      assert.equal(er.domain, d);
      assert.equal(er.domainBound, fn);
      assert.equal(er.domainThrown, false);
      break;

    case 'thrown':
      assert.ok(!er.domainEmitter);
      assert.equal(er.domain, d);
      assert.equal(er.domainThrown, true);
      break;

    case "ENOENT, open 'this file does not exist'":
      assert.equal(er.domain, d);
      assert.equal(er.domainThrown, false);
      assert.equal(typeof er.domainBound, 'function');
      assert.ok(!er.domainEmitter);
      assert.equal(er.code, 'ENOENT');
      assert.equal(er_path, 'this file does not exist');
      assert.equal(typeof er.errno, 'number');
      break;

    case "ENOENT, open 'stream for nonexistent file'":
      assert.equal(typeof er.errno, 'number');
      assert.equal(er.code, 'ENOENT');
      assert.equal(er_path, 'stream for nonexistent file');
      assert.equal(er.domain, d);
      assert.equal(er.domainEmitter, fst);
      assert.ok(!er.domainBound);
      assert.equal(er.domainThrown, false);
      break;

    case 'implicit':
      assert.equal(er.domainEmitter, implicit);
      assert.equal(er.domain, d);
      assert.equal(er.domainThrown, false);
      assert.ok(!er.domainBound);
      break;

    case 'implicit timer':
      assert.equal(er.domain, d);
      assert.equal(er.domainThrown, true);
      assert.ok(!er.domainEmitter);
      assert.ok(!er.domainBound);
      break;

    case 'Cannot read property \'isDirectory\' of undefined':
      assert.equal(er.domain, d);
      assert.ok(!er.domainEmitter);
      assert.ok(!er.domainBound);
      break;

    case 'nextTick execution loop':
      assert.equal(er.domain, d);
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
  assert.equal(caught, expectCaught, 'caught the expected number of errors');
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
    var i = setInterval(function () {
      clearInterval(i);
      setTimeout(function() {
        fs.stat('this file does not exist', function(er, stat) {
          // uh oh!  stat isn't set!
          // pretty common error.
          console.log(stat.isDirectory());
        });
      });
    });
  });
});
expectCaught++;



// implicit addition of a timer created within a domain-bound context.
d.run(function() {
  setTimeout(function() {
    throw new Error('implicit timer');
  });
});
expectCaught++;



// Event emitters added to the domain have their errors routed.
d.add(e);
e.emit('error', new Error('emitted'));
expectCaught++;



// get rid of the `if (er) return cb(er)` malarky, by intercepting
// the cb functions to the domain, and using the intercepted function
// as a callback instead.
function fn(er) {
  throw new Error('This function should never be called!');
  process.exit(1);
}

var bound = d.intercept(fn);
bound(new Error('bound'));
expectCaught++;



// intercepted should never pass first argument to callback
function fn2(data) {
  assert.equal(data, 'data', 'should not be null err argument')
}

var bound = d.intercept(fn2);
bound(null, 'data');

// intercepted should never pass first argument to callback
// even if arguments length is more than 2.
function fn3(data, data2) {
  assert.equal(data, 'data', 'should not be null err argument');
  assert.equal(data2, 'data2', 'should not be data argument');
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
  implicit = new events.EventEmitter;
});

setTimeout(function() {
  // escape from the domain, but implicit is still bound to it.
  implicit.emit('error', new Error('implicit'));
}, 10);
expectCaught++;


var result = d.run(function () {
  return 'return value';
});
assert.equal(result, 'return value');


var fst = fs.createReadStream('stream for nonexistent file')
d.add(fst)
expectCaught++;

[42, null, , false, function(){}, 'string'].forEach(function(something) {
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
