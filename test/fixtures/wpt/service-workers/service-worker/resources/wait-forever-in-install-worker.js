var waitUntilResolve;
self.addEventListener('install', function(event) {
    event.waitUntil(new Promise(function(resolve) {
        waitUntilResolve = resolve;
      }));
  });

self.addEventListener('message', function(event) {
    if (event.data === 'STOP_WAITING') {
      waitUntilResolve();
    }
  });
