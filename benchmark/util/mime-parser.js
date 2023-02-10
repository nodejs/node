'use strict';

const common = require('../common.js');

const inputs = [
  {
    input: 'text/html;charset=gbk',
    output: 'text/html;charset=gbk',
    navigable: true,
    encoding: 'GBK',
  },
  {
    input: 'TEXT/HTML;CHARSET=GBK',
    output: 'text/html;charset=GBK',
    navigable: true,
    encoding: 'GBK',
  },
  {
    input: 'text/html;charset=gbk(',
    output: 'text/html;charset="gbk("',
    navigable: true,
    encoding: null,
  },
  {
    input: 'text/html;x=(;charset=gbk',
    output: 'text/html;x="(";charset=gbk',
    navigable: true,
    encoding: 'GBK',
  },
  {
    input: 'text/html;charset=gbk;charset=windows-1255',
    output: 'text/html;charset=gbk',
    navigable: true,
    encoding: 'GBK',
  },
  {
    input: 'text/html;charset=();charset=GBK',
    output: 'text/html;charset="()"',
    navigable: true,
    encoding: null,
  },
  {
    input: 'text/html;charset =gbk',
    output: 'text/html',
    navigable: true,
    encoding: null,
  },
  {
    input: 'text/html ;charset=gbk',
    output: 'text/html;charset=gbk',
    navigable: true,
    encoding: 'GBK',
  },
  {
    input: 'text/html; charset=gbk',
    output: 'text/html;charset=gbk',
    navigable: true,
    encoding: 'GBK',
  },
  {
    input: 'text/html;charset= gbk',
    output: 'text/html;charset=" gbk"',
    navigable: true,
    encoding: 'GBK',
  },
  {
    input: 'text/html;charset= "gbk"',
    output: 'text/html;charset=" \\"gbk\\""',
    navigable: true,
    encoding: null,
  },
];

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
  //   assert.ok(noDead === undefined || noDead.length > 0);
}
