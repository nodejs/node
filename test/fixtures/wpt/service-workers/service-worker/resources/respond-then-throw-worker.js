var syncport = null;

self.addEventListener('message', function(e) {
  if ('port' in e.data) {
    if (syncport) {
      syncport(e.data.port);
    } else {
      syncport = e.data.port;
    }
  }
});

function sync() {
  return new Promise(function(resolve) {
      if (syncport) {
        resolve(syncport);
      } else {
        syncport = resolve;
      }
    }).then(function(port) {
      port.postMessage('SYNC');
      return new Promise(function(resolve) {
          port.onmessage = function(e) {
            if (e.data === 'ACK') {
              resolve();
            }
          }
        });
    });
}


self.addEventListener('fetch', function(event) {
    // In Firefox the result would depend on a race between fetch handling
    // and exception handling code. On the assumption that this might be a common
    // design error, we explicitly allow the exception to be handled first.
    event.respondWith(sync().then(() => new Response('intercepted')));

    throw("error");
  });
