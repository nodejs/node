self.addEventListener('error', function(event) {});
self.addEventListener('activate', function(event) { throw new Error(); });
