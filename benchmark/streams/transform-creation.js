'use strict';
const common = require('../common.js');
const Transform = require('stream').Transform;
const inherits = require('util').inherits;

const bench = common.createBenchmark(main, {
  n: [1e6]
});

function MyTransform() {
  Transform.call(this);
}
inherits(MyTransform, Transform);
MyTransform.prototype._transform = function() {};

function main({ n }) {
  bench.start();
  for (var i = 0; i < n; ++i)
    new MyTransform();
  bench.end(n);
}
