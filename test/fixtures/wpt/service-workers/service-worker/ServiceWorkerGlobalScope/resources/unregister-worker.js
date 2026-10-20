function matchQuery(query) {
  return self.location.href.indexOf(query) != -1;
}

if (matchQuery('?evaluation'))
  self.registration.unregister();

self.addEventListener('install', function(e) {
    if (matchQuery('?install')) {
      // Don't do waitUntil(unregister()) as that would deadlock as specified.
      self.registration.unregister();
    }
  });

self.addEventListener('activate', function(e) {
    if (matchQuery('?activate'))
      e.waitUntil(self.registration.unregister());
  });

self.addEventListener('message', function(e) {
    e.waitUntil(self.registration.unregister()
      .then(function(result) {
          e.data.port.postMessage({result: result});
        }));
  });
