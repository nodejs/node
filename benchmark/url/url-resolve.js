'use strict';
const common = require('../common.js');
const url = require('url');
const v8 = require('v8');
const hrefs = require('../fixtures/url-inputs.js').urls;
hrefs.noscheme = 'some.ran/dom/url.thing?oh=yes#whoo';

const paths = {
  'up': '../../../../../etc/passwd',
  'sibling': '../foo/bar?baz=boom',
  'foo/bar': 'foo/bar',
  'withscheme': 'http://nodejs.org',
  'down': './foo/bar?baz'
};

const bench = common.createBenchmark(main, {
  href: Object.keys(hrefs),
  path: Object.keys(paths),
  n: [1e5]
});

function main(conf) {
  const n = conf.n | 0;
  const href = hrefs[conf.href];
  const path = paths[conf.path];

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
