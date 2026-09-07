'use strict';

self.onfetch = function(event) {
  if (event.request.url.indexOf('non-existent-file.txt') !== -1) {
    event.respondWith(new Response('Response from service worker'));
  }
};
