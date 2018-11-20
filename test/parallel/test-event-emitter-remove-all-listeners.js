var common = require('../common');
var assert = require('assert');
var events = require('events');


function expect(expected) {
  var actual = [];
  process.on('exit', function() {
    assert.deepEqual(actual.sort(), expected.sort());
  });
  function listener(name) {
    actual.push(name)
  }
  return common.mustCall(listener, expected.length);
}

function listener() {}

var e1 = new events.EventEmitter();
e1.on('foo', listener);
e1.on('bar', listener);
e1.on('baz', listener);
e1.on('baz', listener);
var fooListeners = e1.listeners('foo');
var barListeners = e1.listeners('bar');
var bazListeners = e1.listeners('baz');
e1.on('removeListener', expect(['bar', 'baz', 'baz']));
e1.removeAllListeners('bar');
e1.removeAllListeners('baz');
assert.deepEqual(e1.listeners('foo'), [listener]);
assert.deepEqual(e1.listeners('bar'), []);
assert.deepEqual(e1.listeners('baz'), []);
// after calling removeAllListeners,
// the old listeners array should stay unchanged
assert.deepEqual(fooListeners, [listener]);
assert.deepEqual(barListeners, [listener]);
assert.deepEqual(bazListeners, [listener, listener]);
// after calling removeAllListeners,
// new listeners arrays are different from the old
assert.notEqual(e1.listeners('bar'), barListeners);
assert.notEqual(e1.listeners('baz'), bazListeners);

var e2 = new events.EventEmitter();
e2.on('foo', listener);
e2.on('bar', listener);
// expect LIFO order
e2.on('removeListener', expect(['foo', 'bar', 'removeListener']));
e2.on('removeListener', expect(['foo', 'bar']));
e2.removeAllListeners();
console.error(e2);
assert.deepEqual([], e2.listeners('foo'));
assert.deepEqual([], e2.listeners('bar'));

var e3 = new events.EventEmitter();
e3.on('removeListener', listener);
// check for regression where removeAllListeners throws when
// there exists a removeListener listener, but there exists
// no listeners for the provided event type
assert.doesNotThrow(e3.removeAllListeners.bind(e3, 'foo'));

var e4 = new events.EventEmitter();
var expectLength = 2;
e4.on('removeListener', function(name, listener) {
  assert.equal(expectLength--, this.listeners('baz').length);
});
e4.on('baz', function(){});
e4.on('baz', function(){});
e4.on('baz', function(){});
assert.equal(e4.listeners('baz').length, expectLength+1);
e4.removeAllListeners('baz');
assert.equal(e4.listeners('baz').length, 0);
