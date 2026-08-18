var events_seen = [];

self.registration.addEventListener('updatefound', function() {
    events_seen.push('updatefound');
  });

self.addEventListener('activate', function(e) {
    events_seen.push('activate');
  });

self.addEventListener('fetch', function(e) {
    events_seen.push('fetch');
    e.respondWith(new Response(events_seen));
  });

self.addEventListener('message', function(e) {
    events_seen.push('message');
    self.registration.update();
  });

// update() during the script evaluation should be ignored.
self.registration.update();
