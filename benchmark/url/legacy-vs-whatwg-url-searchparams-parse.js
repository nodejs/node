'use strict';
const common = require('../common.js');
const { URLSearchParams } = require('url');
const querystring = require('querystring');

const inputs = {
  noencode: 'foo=bar&baz=quux&xyzzy=thud',
  encodemany: '%66%6F%6F=bar&%62%61%7A=quux&xyzzy=%74h%75d',
  encodefake: 'foo=%©ar&baz=%A©uux&xyzzy=%©ud',
  encodelast: 'foo=bar&baz=quux&xyzzy=thu%64',
  multicharsep: 'foo=bar&&&&&&&&&&baz=quux&&&&&&&&&&xyzzy=thud',
  multivalue: 'foo=bar&foo=baz&foo=quux&quuy=quuz',
  multivaluemany: 'foo=bar&foo=baz&foo=quux&quuy=quuz&foo=abc&foo=def&' +
                  'foo=ghi&foo=jkl&foo=mno&foo=pqr&foo=stu&foo=vwxyz',
  manypairs: 'a&b&c&d&e&f&g&h&i&j&k&l&m&n&o&p&q&r&s&t&u&v&w&x&y&z'
};

const bench = common.createBenchmark(main, {
  type: Object.keys(inputs),
  method: ['legacy', 'whatwg'],
  n: [1e5]
});

function useLegacy(n, input) {
  querystring.parse(input);
  bench.start();
  for (var i = 0; i < n; i += 1) {
    querystring.parse(input);
  }
  bench.end(n);
}

function useWHATWG(n, input) {
  new URLSearchParams(input);
  bench.start();
  for (var i = 0; i < n; i += 1) {
    new URLSearchParams(input);
  }
  bench.end(n);
}

function main(conf) {
  const type = conf.type;
  const n = conf.n | 0;
  const method = conf.method;

  const input = inputs[type];
  if (!input) {
    throw new Error('Unknown input type');
  }

  switch (method) {
    case 'legacy':
      useLegacy(n, input);
      break;
    case 'whatwg':
      useWHATWG(n, input);
      break;
    default:
      throw new Error('Unknown method');
  }
}
