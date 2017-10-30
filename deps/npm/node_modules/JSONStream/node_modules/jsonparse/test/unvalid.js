var test = require('tape');
var Parser = require('../');

test('unvalid', function (t) {
  var count = 0;

  var p = new Parser();
  p.onError = function (value) {
    count++;
    t.equal(1, count);
    t.end();
  };

  p.write('{"test": eer[');
});
