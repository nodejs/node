function activateWorkerImmediately(event) {
  event.waitUntil(self.skipWaiting())
}

function becomeAvailableToAllPages(event) {
  event.waitUntil(self.clients.claim())
}

function cachedResponseOrDoRequest(event) {
  event.respondWith(
    caches.open('v1')
      .then(cache => caches.match(event.request))
      .then(cachedResponse => cachedResponse || fetch(event.request))
  )
}

self.addEventListener('install', activateWorkerImmediately)
self.addEventListener('activate', becomeAvailableToAllPages)
self.addEventListener('fetch', cachedResponseOrDoRequest)
