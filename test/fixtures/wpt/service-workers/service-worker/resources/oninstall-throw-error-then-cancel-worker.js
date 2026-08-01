self.onerror = function(event) { return true; };

self.addEventListener('install', function(event) { throw new Error(); });
