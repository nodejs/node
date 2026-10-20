self.addEventListener('error', function(event) {});
self.addEventListener('install', function(event) { throw new Error(); });
