self.addEventListener('activate', event => {
    event.waitUntil(
        Promise.all[
            self.registration.navigationPreload.enable(),
            self.registration.navigationPreload.setHeaderValue('hello')]);
  });

self.addEventListener('fetch', event => {
    event.respondWith(event.preloadResponse);
  });
