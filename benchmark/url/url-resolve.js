'use strict';
var common = require('../common.js');
var url = require('url');
var v8 = require('v8');

var hrefs = [
  'http://example.com/',
  'http://nodejs.org/docs/latest/api/url.html#url_url_format_urlobj',
  'http://blog.nodejs.org/',
  'https://encrypted.google.com/search?q=url&q=site:npmjs.org&hl=en',
  'javascript:alert("node is awesome");',
  'some.ran/dom/url.thing?oh=yes#whoo'
];


var paths = [
  '../../../../../etc/passwd',
  '../foo/bar?baz=boom',
  'foo/bar',
  'http://nodejs.org',
  './foo/bar?baz'
];

var bench = common.createBenchmark(main, {
  href: Object.keys(hrefs),
  path: Object.keys(paths),
  n: [1e5]
});

function main(conf) {
  var n = conf.n | 0;
  var href = hrefs[conf.href];
  var path = paths[conf.path];

  // Force-optimize url.resolve() so that the benchmark doesn't get
  // disrupted by the optimizer kicking in halfway through.
  url.resolve(href, path);
  v8.setFlagsFromString('--allow_natives_syntax');
  eval('%OptimizeFunctionOnNextCall(url.resolve)');

  bench.start();
  for (var i = 0; i < n; i += 1)
    url.resolve(href, path);
  bench.end(n);
}
