self.addEventListener('activate', event => {
    event.waitUntil(
        self.registration.navigationPreload.enable());
  });

self.addEventListener('fetch', event => {
    event.respondWith(event.preloadResponse);
  });
