var test = require('tape');
var Parser = require('../');

test('3 bytes of utf8', function (t) {
  t.plan(1);

  var p = new Parser();
  p.onValue = function (value) {
    t.equal(value, '├──');
  };

  p.write('"├──"');
});

test('utf8 snowman', function (t) {
  t.plan(1);

  var p = new Parser();
  p.onValue = function (value) {
    t.equal(value, '☃');
  };

  p.write('"☃"');
});

test('utf8 with regular ascii', function (t) {
  t.plan(4);

  var p = new Parser();
  var expected = [ "snow: ☃!", "xyz", "¡que!" ];
  expected.push(expected.slice());

  p.onValue = function (value) {
    t.deepEqual(value, expected.shift());
  };

  p.write('["snow: ☃!","xyz","¡que!"]');
});
