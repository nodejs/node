'use strict';

self.addEventListener('activate', event => {
  event.waitUntil(new Promise(() => {
        // Use a promise that never resolves to prevent this service worker from
        // advancing past the 'activating' state.
      }));
});
