var test = require('tape');
var Parser = require('../');

test('parse surrogate pair', function (t) {
  t.plan(1);

  var p = new Parser();
  p.onValue = function (value) {
    t.equal(value, '😋');
  };

  p.write('"\\uD83D\\uDE0B"');
});

test('parse chunked surrogate pair', function (t) {
  t.plan(1);

  var p = new Parser();
  p.onValue = function (value) {
    t.equal(value, '😋');
  };

  p.write('"\\uD83D');
  p.write('\\uDE0B"');
});
