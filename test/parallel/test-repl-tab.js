var assert = require('assert');
var util = require('util');
var repl = require('repl');
var zlib = require('zlib');
var defaultScopes = [
  'Array',
  'Boolean',
  'Date',
  'Error',
  'EvalError',
  'Function',
  'Infinity',
  'JSON',
  'Math',
  'NaN',
  'Number',
  'Object',
  'RangeError',
  'ReferenceError',
  'RegExp',
  'String',
  'SyntaxError',
  'TypeError',
  'URIError',
  'decodeURI',
  'decodeURIComponent',
  'encodeURI',
  'encodeURIComponent',
  'eval',
  'isFinite',
  'isNaN',
  'parseFloat',
  'parseInt',
  'undefined'
];

// just use builtin stream inherited from Duplex
var putIn = zlib.createGzip();
var testMe = repl.start('', putIn, function(cmd, context, filename, callback) {
  callback(null, cmd);
});

testMe._domain.on('error', function (e) {
  assert.fail();
});

testMe.complete('', function(err, results) {
  assert.equal(err, null);
  assert.deepEqual(results[0], defaultScopes);
});
