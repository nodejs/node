// This worker intercepts a request for EMBED/OBJECT and responds with a
// response that indicates that interception occurred. The tests expect
// that interception does not occur.
self.addEventListener('fetch', e => {
    if (e.request.url.indexOf('embedded-content-from-server.html') != -1) {
      e.respondWith(fetch('embedded-content-from-service-worker.html'));
      return;
    }

    if (e.request.url.indexOf('green.png') != -1) {
      e.respondWith(Promise.reject('network error to show interception occurred'));
      return;
    }
  });
