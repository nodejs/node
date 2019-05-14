'use strict';
const common = require('../common.js');
const url = require('url');
const hrefs = common.urls;
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

function main({ n, href, path }) {
  const h = hrefs[href];
  const p = paths[path];

  bench.start();
  for (var i = 0; i < n; i += 1)
    url.resolve(h, p);
  bench.end(n);
}
