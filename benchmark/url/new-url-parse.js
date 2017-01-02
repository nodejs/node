'use strict';
const common = require('../common.js');
const url = require('url');
const v8 = require('v8');

const bench = common.createBenchmark(main, {
  type: 'one two three four five'.split(' '),
  method: ['old', 'new'],
  n: [25e4]
});

function useOld(n, input) {
  // Force-optimize url.parse() so that the benchmark doesn't get
  // disrupted by the optimizer kicking in halfway through.
  url.parse(input);
  v8.setFlagsFromString('--allow_natives_syntax');
  eval('%OptimizeFunctionOnNextCall(url.parse)');

  bench.start();
  for (var i = 0; i < n; i += 1)
    url.parse(input);
  bench.end(n);
}

function useNew(n, input) {
  bench.start();
  for (var i = 0; i < n; i += 1)
    new url.URL(input);
  bench.end(n);
}

function main(conf) {
  const type = conf.type;
  const n = conf.n | 0;
  const method = conf.method;

  var inputs = {
    one: 'http://nodejs.org/docs/latest/api/url.html#url_url_format_urlobj',
    two: 'http://blog.nodejs.org/',
    three: 'https://encrypted.google.com/search?q=url&q=site:npmjs.org&hl=en',
    four: 'javascript:alert("node is awesome");',
    //five: 'some.ran/dom/url.thing?oh=yes#whoo',
    five: 'https://user:pass@example.com/',
  };
  var input = inputs[type] || '';

  switch (method) {
    case 'old':
      useOld(n, input);
      break;
    case 'new':
      useNew(n, input);
      break;
    default:
      throw new Error('Unknown method');
  }
}
