'use strict';

const common = require('../common');
const util = require('util');
const fs = require('fs');
const zlib = require('zlib');
const dns = require('dns');

const bench = common.createBenchmark(main, {
  n: [4e6],
  fn: [ 'fsStat', 'zlibGzip', 'dnsLookup', 'timersSetTimeout' ]
});

function main({ n, fn }) {
  switch (fn) {
    case 'fsStat':
      fsStat(n).catch(console.log);
      break;
    case 'zlibGzip':
      zlibGzip(n).catch(console.log);
      break;
    case 'dnsLookup':
      dnsLookup(n).catch(console.log);
      break;
    case 'timersSetTimeout':
      timersSetTimeout(n).catch(console.log);
      break;
  }
}

// stat this file repeatedly
async function fsStat(n) {
  const stat = util.promisify(fs.stat);
  let remaining = n;
  bench.start();
  while (remaining-- > 0)
    await stat(__filename);
  bench.end(n);
}

// compress this file repeatedly
async function zlibGzip(n) {
  const readFile = util.promisify(fs.readFile);
  const fileContent = await readFile(__filename);

  const gzip = util.promisify(zlib.gzip);
  let remaining = n;
  bench.start();
  while (remaining-- > 0)
    await gzip(fileContent);
  bench.end(n);
}

// dns lookup localhost, promisified call with two args and a unique cb
// signature that gets promisified into an object with the properties
// {address, family}
async function dnsLookup(n) {
  const lookup = util.promisify(dns.lookup);
  let remaining = n;
  bench.start();
  while (remaining-- > 0)
    await lookup('127.0.0.1', { all: true });
  bench.end(n);
}

// promisified timeout benchmark
// useful because timers don't implement standard cb first style
// so they make use of the `util.promisify.custom` symbol
// which it is useful to benchmark
async function timersSetTimeout(n) {
  const setTimeoutPromified = util.promisify(setTimeout);
  let remaining = n;
  bench.start();
  while (remaining-- > 0)
    await setTimeoutPromified(1);
  bench.end(n);
}
