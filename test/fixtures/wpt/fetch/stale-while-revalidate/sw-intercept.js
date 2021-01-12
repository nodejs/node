async function broadcast(msg) {
  for (const client of await clients.matchAll()) {
    client.postMessage(msg);
  }
}

self.addEventListener('fetch', event => {
  event.waitUntil(broadcast(event.request.url));
  event.respondWith(fetch(event.request));
});

self.addEventListener('activate', event => {
  self.clients.claim();
});
