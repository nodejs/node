// Ensure we can handle multiple activate handlers. One handler throwing an
// error should cause the event dispatch to be treated as having unhandled
// errors.
self.addEventListener('activate', function(event) {});
self.addEventListener('activate', function(event) {});
self.addEventListener('activate', function(event) { throw new Error(); });
self.addEventListener('activate', function(event) {});
