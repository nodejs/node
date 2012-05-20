var url = require('url'),
    urls = [
      'http://nodejs.org/docs/latest/api/url.html#url_url_format_urlobj',
      'http://blog.nodejs.org/',
      'https://encrypted.google.com/search?q=url&q=site:npmjs.org&hl=en',
      'javascript:alert("node is awesome");',
      'some.ran/dom/url.thing?oh=yes#whoo'
    ],
    paths = [
      '../foo/bar?baz=boom',
      'foo/bar',
      'http://nodejs.org',
      './foo/bar?baz'
    ];

urls.forEach(url.parse);
urls.forEach(url.format);
urls.forEach(function(u){
  paths.forEach(function(p){
    url.resolve(u, p);
  });
});