self.addEventListener('activate', event => {
    event.waitUntil(
        self.registration.navigationPreload.enable());
  });

self.addEventListener('fetch', event => {
    event.respondWith(
      event.preloadResponse
        .then(res => res.text())
        .then(text => {
            return new Response(
                '<body>[' + text + ']</body>',
                {headers: [['content-type', 'text/html']]});
          }));
  });
