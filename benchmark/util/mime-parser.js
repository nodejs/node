'use strict';

const common = require('../common.js');

const strings = [
  'text/html;charset=gbk',
  'text/html;charset=GBK',
  'text/html;charset=gbk;charset=windows-1255',
  'text/html; charset=gbk',
  'text/html;charset= "gbk"',
  'text/html ;charset=gbk',
  'application/json; charset="utf-8"',
  'text/html;x=(;charset=gbk',
];

const bench = common.createBenchmark(main, {
  strings,
  n: [1e5],
});

function main({ strings: string, n }) {
  const { MIMEType } = require('util');

  bench.start();
  new MIMEType(string);
  bench.end(n);
}
