self.onerror = function(event) { return true; };

self.addEventListener('activate', function(event) { throw new Error(); });
