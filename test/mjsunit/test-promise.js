process.mixin(require('./common'));
var
  Promise = require('events').Promise,

  TEST_VALUE = {some: 'object'},

  expectedCallbacks = {
    a1: 1,
    a2: 1,
    b1: 1,
    b2: 1,
  };

// Test regular & late callback binding
var a = new Promise();
a.addCallback(function(value) {
  assert.equal(TEST_VALUE, value);
  expectedCallbacks.a1--;
});
a.emitSuccess(TEST_VALUE);

a.addCallback(function(value) {
  assert.equal(TEST_VALUE, value);
  expectedCallbacks.a2--;
});

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