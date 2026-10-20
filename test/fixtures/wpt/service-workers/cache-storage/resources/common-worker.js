self.onmessage = function(e) {
    var cache_name = e.data.name;

    self.caches.open(cache_name)
        .then(function(cache) {
            return Promise.all([
                cache.put('https://example.com/a', new Response('a')),
                cache.put('https://example.com/b', new Response('b')),
                cache.put('https://example.com/c', new Response('c'))
            ]);
        })
        .then(function() {
            self.postMessage('ok');
        });
};
