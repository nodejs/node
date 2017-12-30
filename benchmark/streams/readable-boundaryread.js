'use strict';

const common = require('../common');
const Readable = require('stream').Readable;

const bench = common.createBenchmark(main, {
  n: [200e1],
  type: ['string', 'buffer']
});

function main({ n, type }) {
  const s = new Readable();
  var data = 'a'.repeat(32);
  if (type === 'buffer')
    data = Buffer.from(data);
  s._read = function() {};

  bench.start();
  for (var k = 0; k < n; ++k) {
    for (var i = 0; i < 1e4; ++i)
      s.push(data);
    while (s.read(32));
  }
  bench.end(n);
}
