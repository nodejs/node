self.urls = [];
self.addEventListener('fetch', function(event) {
    self.urls.push(event.request.url);
  });
self.addEventListener('message', function(event) {
    event.data.port.postMessage({urls: self.urls});
  });
