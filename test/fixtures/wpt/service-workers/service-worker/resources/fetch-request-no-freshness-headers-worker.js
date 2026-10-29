var requests = [];

self.addEventListener('message', function(event) {
    event.data.port.postMessage({requests: requests});
  });

self.addEventListener('fetch', function(event) {
    var url = event.request.url;
    var headers = [];
    for (var header of event.request.headers) {
      headers.push(header);
    }
    requests.push({
        url: url,
        headers: headers
      });
    event.respondWith(fetch(event.request));
  });
