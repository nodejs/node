var requests = [];

self.addEventListener('message', function(event) {
    event.data.port.postMessage({requests: requests});
    requests = [];
  });

self.addEventListener('fetch', function(event) {
    requests.push({
        url: event.request.url,
        mode: event.request.mode
      });
  });
