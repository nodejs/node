self.addEventListener('fetch', function(event) {
    var url = event.request.url;
    if (url.indexOf('sample?test') == -1) {
      return;
    }
    event.respondWith(new Promise(function(resolve) {
        var headers = new Headers;
        headers.append('foo', 'foo');
        headers.append('foo', 'b\0r'); // header value with a null byte
        resolve(new Response('hello world', {'headers': headers}));
      }));
  });
