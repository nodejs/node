var result = null;

self.addEventListener('message', function(event) {
    event.data.port.postMessage(result);
  });

self.addEventListener('fetch', function(event) {
    if (!result)
      result = 'PASS';
    event.respondWith(new Response());
  });

self.addEventListener('fetch', function(event) {
    result = 'FAIL: fetch event propagated';
  });
