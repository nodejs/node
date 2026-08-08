var saw_activate_event = false

self.addEventListener('activate', function() {
    saw_activate_event = true;
  });

self.addEventListener('message', function(event) {
    var port = event.data.port;
    event.waitUntil(self.skipWaiting()
      .then(function(result) {
          if (result !== undefined) {
            port.postMessage('FAIL: Promise should be resolved with undefined');
            return;
          }

          if (!saw_activate_event) {
            port.postMessage(
                'FAIL: Promise should be resolved after activate event is dispatched');
            return;
          }

          if (self.registration.active.state !== 'activating') {
            port.postMessage(
                'FAIL: Promise should be resolved before ServiceWorker#state is set to activated');
            return;
          }

          port.postMessage('PASS');
        })
      .catch(function(e) {
          port.postMessage('FAIL: unexpected exception: ' + e);
        }));
  });
