// Ensure we can handle multiple install handlers. One handler throwing an
// error should cause the event dispatch to be treated as having unhandled
// errors.
self.addEventListener('install', function(event) {});
self.addEventListener('install', function(event) {});
self.addEventListener('install', function(event) { throw new Error(); });
self.addEventListener('install', function(event) {});
