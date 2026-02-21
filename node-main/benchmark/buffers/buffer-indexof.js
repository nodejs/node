'use strict';
const common = require('../common.js');
const fs = require('fs');
const path = require('path');

const searchStrings = [
  '@',
  'SQ',
  '--l',
  'Alice',
  'Gryphon',
  'Ou est ma chatte?',
  'found it very',
  'neighbouring pool',
  'aaaaaaaaaaaaaaaaa',
  'venture to go near the house till she had brought herself down to',
  '</i> to the Caterpillar',
];

const bench = common.createBenchmark(main, {
  search: searchStrings,
  encoding: ['undefined', 'utf8', 'ucs2'],
  type: ['buffer', 'string'],
  n: [5e4],
}, {
  combinationFilter: (p) => {
    return (p.type === 'buffer' && p.encoding === 'undefined') ||
           (p.type !== 'buffer' && p.encoding !== 'undefined');
  },
});

function main({ n, search, encoding, type }) {
  let aliceBuffer = fs.readFileSync(
    path.resolve(__dirname, '../fixtures/alice.html'),
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
  for (let i = 0; i < n; i++) {
    aliceBuffer.indexOf(search, 0, encoding);
  }
  bench.end(n);
}
