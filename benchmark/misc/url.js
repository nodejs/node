var url = require('url')
var n = 25 * 100;

var urls = [
  'http://nodejs.org/docs/latest/api/url.html#url_url_format_urlobj',
  'http://blog.nodejs.org/',
  'https://encrypted.google.com/search?q=url&q=site:npmjs.org&hl=en',
  'javascript:alert("node is awesome");',
  'some.ran/dom/url.thing?oh=yes#whoo'
];

var paths = [
  '../foo/bar?baz=boom',
  'foo/bar',
  'http://nodejs.org',
  './foo/bar?baz'
];

benchmark('parse()', url.parse);
benchmark('format()', url.format);
paths.forEach(function(p) {
  benchmark('resolve("' + p + '")', function(u) {
    url.resolve(u, p)
  });
});

function benchmark(name, fun) {
  var timestamp = process.hrtime();
  for (var i = 0; i < n; ++i) {
    for (var j = 0, k = urls.length; j < k; ++j) fun(urls[j]);
  }
  timestamp = process.hrtime(timestamp);

  var seconds = timestamp[0];
  var nanos = timestamp[1];
  var time = seconds + nanos / 1e9;
  var rate = n / time;

  console.log('misc/url.js %s: %s', name, rate.toPrecision(5));
}
