process.mixin(require('./common'));
var
  Promise = require('events').Promise,

  TEST_VALUE = {some: 'object'},

  expectedCallbacks = {
    a1: 1,
    a2: 1,
    b1: 1,
    b2: 1,
    c1: 1,
    d1: 1,
  };

// Test regular & late callback binding
var a = new Promise();
a.addCallback(function(value) {
  assert.equal(TEST_VALUE, value);
  expectedCallbacks.a1--;
});
a.addErrback(function(error) {
  assert.notEqual(TEST_VALUE, error, 'normal');
});
a.emitSuccess(TEST_VALUE);

assert.ok(a.addCallback(function(value) {
  assert.equal(TEST_VALUE, value);
  expectedCallbacks.a2--;
}));
assert.ok(a.addErrback(function(error) {
  assert.notEqual(TEST_VALUE, error, 'late');
}));

// Test regular & late errback binding
var b = new Promise();
b.addErrback(function(value) {
  assert.equal(TEST_VALUE, value);
  expectedCallbacks.b1--;
});
b.emitError(TEST_VALUE);

b.addErrback(function(value) {
  assert.equal(TEST_VALUE, value);
  expectedCallbacks.b2--;
});

// Test late errback binding
var c = new Promise();
c.emitError(TEST_VALUE);
assert.ok(c.addErrback(function(value) {
  assert.equal(TEST_VALUE, value);
  expectedCallbacks.c1--;
}));

// Test errback exceptions
var d = new Promise();
d.emitError(TEST_VALUE);

process.addListener('uncaughtException', function(e) {
  if (e.name === "AssertionError") {
    throw e;
  }

  expectedCallbacks.d1--;
  assert.ok(e.message.match(/unhandled emitError/i));
});

process.addListener('exit', function() {
  for (var name in expectedCallbacks) {
    var count = expectedCallbacks[name];

    assert.equal(
      0,
      count,
      'Callback '+name+' fire count off by: '+count
    );
  }
});