'use strict';
const common = require('../common.js');
const URL = require('url').URL;
const inputs = require('../fixtures/url-inputs.js').urls;

const bench = common.createBenchmark(main, {
  input: Object.keys(inputs),
  prop: ['href', 'origin', 'protocol',
         'username', 'password', 'host', 'hostname', 'port',
         'pathname', 'search', 'searchParams', 'hash'],
  n: [3e5]
});

function setAndGet(n, url, prop, alternative) {
  const old = url[prop];
  bench.start();
  for (var i = 0; i < n; i += 1) {
    url[prop] = n % 2 === 0 ? alternative : old;  // set
    url[prop];  // get
  }
  bench.end(n);
}

function get(n, url, prop) {
  bench.start();
  for (var i = 0; i < n; i += 1) {
    url[prop];  // get
  }
  bench.end(n);
}

const alternatives = {
  href: 'http://user:pass@foo.bar.com:21/aaa/zzz?l=25#test',
  protocol: 'https:',
  username: 'user2',
  password: 'pass2',
  host: 'foo.bar.net:22',
  hostname: 'foo.bar.org',
  port: '23',
  pathname: '/aaa/bbb',
  search: '?k=99',
  hash: '#abcd'
};

function getAlternative(prop) {
  return alternatives[prop];
}

function main(conf) {
  const n = conf.n | 0;
  const input = inputs[conf.input];
  const url = new URL(input);
  const prop = conf.prop;

  switch (prop) {
    case 'protocol':
    case 'username':
    case 'password':
    case 'host':
    case 'hostname':
    case 'port':
    case 'pathname':
    case 'search':
    case 'hash':
    case 'href':
      setAndGet(n, url, prop, getAlternative(prop));
      break;
    case 'origin':
    case 'searchParams':
      get(n, url, prop);
      break;
    default:
      throw new Error('Unknown prop');
  }
}
