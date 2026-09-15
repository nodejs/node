importScripts('/resources/testharness.js');

self.addEventListener('fetch', function(event) {
    event.respondWith(new Promise(function(resolve) {
      Promise.resolve()
        .then(function() {
            assert_equals(
                event.request.redirect, 'manual',
                'The redirect mode of navigation request must be manual.');
            return fetch(event.request);
          })
        .then(function(response) {
            assert_equals(
                response.type, 'opaqueredirect',
                'The response type of 302 response must be opaqueredirect.');
            resolve(new Response('OK'));
          })
        .catch(function(error) {
            resolve(new Response('Failed in SW: ' + error));
          });
    }));
  });
