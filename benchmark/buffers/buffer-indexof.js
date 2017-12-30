'use strict';
const common = require('../common.js');
const fs = require('fs');
const path = require('path');

const searchStrings = [
  '@',
  'SQ',
  '10x',
  '--l',
  'Alice',
  'Gryphon',
  'Panther',
  'Ou est ma chatte?',
  'found it very',
  'among mad people',
  'neighbouring pool',
  'Soo--oop',
  'aaaaaaaaaaaaaaaaa',
  'venture to go near the house till she had brought herself down to',
  '</i> to the Caterpillar'
];

const bench = common.createBenchmark(main, {
  search: searchStrings,
  encoding: ['undefined', 'utf8', 'ucs2', 'binary'],
  type: ['buffer', 'string'],
  iter: [100000]
});

function main({ iter, search, encoding, type }) {
  var aliceBuffer = fs.readFileSync(
    path.resolve(__dirname, '../fixtures/alice.html')
  );

  if (encoding === 'undefined') {
    encoding = undefined;
  }

  if (encoding === 'ucs2') {
    aliceBuffer = Buffer.from(aliceBuffer.toString(), encoding);
  }

  if (type === 'buffer') {
    search = Buffer.from(Buffer.from(search).toString(), encoding);
  }

  bench.start();
  for (var i = 0; i < iter; i++) {
    aliceBuffer.indexOf(search, 0, encoding);
  }
  bench.end(iter);
}
