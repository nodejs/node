importScripts('worker-testharness.js');

this.addEventListener('fetch', function(event) {
    event.respondWith(new Response('ERROR'));
  });
