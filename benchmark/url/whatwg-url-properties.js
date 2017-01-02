'use strict';

var common = require('../common.js');
var URL = require('url').URL;

var bench = common.createBenchmark(main, {
  url: [
    'http://example.com/',
    'https://encrypted.google.com/search?q=url&q=site:npmjs.org&hl=en',
    'javascript:alert("node is awesome");',
    'http://user:pass@foo.bar.com:21/aaa/zzz?l=24#test'
  ],
  prop: ['toString', 'href', 'origin', 'protocol',
    'username', 'password', 'host', 'hostname', 'port',
    'pathname', 'search', 'searchParams', 'hash'],
  n: [1e4]
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

function stringify(n, url, prop) {
  bench.start();
  for (var i = 0; i < n; i += 1) {
    url.toString();
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
  const url = new URL(conf.url);
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
      setAndGet(n, url, prop, getAlternative(prop));
      break;
    // TODO: move href to the first group when the setter lands.
    case 'href':
    case 'origin':
    case 'searchParams':
      get(n, url, prop);
      break;
    case 'toString':
      stringify(n, url);
      break;
    default:
      throw new Error('Unknown prop');
  }
}
