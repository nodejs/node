'use strict';
var common = require('../common.js');
var Transform = require('stream').Transform;
var inherits = require('util').inherits;

var bench = common.createBenchmark(main, {
  n: [1e6]
});

function MyTransform() {
  Transform.call(this);
}
inherits(MyTransform, Transform);
MyTransform.prototype._transform = function() {};

function main(conf) {
  var n = +conf.n;

  bench.start();
  for (var i = 0; i < n; ++i)
    new MyTransform();
  bench.end(n);
}
