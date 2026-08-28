self.addEventListener('fetch', function(event) {
    var url = event.request.url;
    if (url.indexOf('sample?test') == -1) {
      return;
    }

    event.respondWith(new Promise(function(resolve) {
        var headers = new Headers;
        headers.append('TEST', 'ßÀ¿'); // header value holds the Latin1 (ISO8859-1) string.
        resolve(new Response('hello world', {'headers': headers}));
      }));
  });
