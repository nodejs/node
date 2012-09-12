var util = require('util');
var url = require('url')

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
  benchmark('resolve("' + p + '")', function(u) { url.resolve(u, p) });
});

function benchmark(name, fun) {
  process.stdout.write('benchmarking ' + name + ' ... ');

  var timestamp = process.hrtime();
  for (var i = 0; i < 25 * 1000; ++i) {
    for (var j = 0, k = urls.length; j < k; ++j) fun(urls[j]);
  }
  timestamp = process.hrtime(timestamp);

  var seconds = timestamp[0];
  var millis = timestamp[1]; // actually nanoseconds
  while (millis > 1000) millis /= 10;
  var time = (seconds * 1000 + millis) / 1000;

  process.stdout.write(util.format('%s sec\n', time.toFixed(3)));
}
