'use strict';
var common = require('../common.js');
var url = require('url');
var v8 = require('v8');

var bench = common.createBenchmark(main, {
  type: 'one two three four five six'.split(' '),
  n: [25e4]
});

function main(conf) {
  var type = conf.type;
  var n = conf.n | 0;

  var inputs = {
    one: 'http://nodejs.org/docs/latest/api/url.html#url_url_format_urlobj',
    two: 'http://blog.nodejs.org/',
    three: 'https://encrypted.google.com/search?q=url&q=site:npmjs.org&hl=en',
    four: 'javascript:alert("node is awesome");',
    five: 'some.ran/dom/url.thing?oh=yes#whoo',
    six: 'https://user:pass@example.com/',
  };
  var input = inputs[type] || '';

  // Force-optimize url.parse() so that the benchmark doesn't get
  // disrupted by the optimizer kicking in halfway through.
  for (var name in inputs)
    url.parse(inputs[name]);

  v8.setFlagsFromString('--allow_natives_syntax');
  eval('%OptimizeFunctionOnNextCall(url.parse)');

  bench.start();
  for (var i = 0; i < n; i += 1)
    url.parse(input);
  bench.end(n);
}
