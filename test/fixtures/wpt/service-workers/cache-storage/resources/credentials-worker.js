var cache_name = 'credentials';

function assert_equals(actual, expected, message) {
  if (!Object.is(actual, expected))
    throw Error(message + ': expected: ' + expected + ', actual: ' + actual);
}

self.onfetch = function(e) {
  if (!/\.txt$/.test(e.request.url)) return;
  var content = e.request.url;
  var cache;
  e.respondWith(
    self.caches.open(cache_name)
      .then(function(result) {
        cache = result;
        return cache.put(e.request, new Response(content));
      })

      .then(function() { return cache.match(e.request); })
      .then(function(result) { return result.text(); })
      .then(function(text) {
        assert_equals(text, content, 'Cache.match() body should match');
      })

      .then(function() { return cache.matchAll(e.request); })
      .then(function(results) {
        assert_equals(results.length, 1, 'Should have one response');
        return results[0].text();
      })
      .then(function(text) {
        assert_equals(text, content, 'Cache.matchAll() body should match');
      })

      .then(function() { return self.caches.match(e.request); })
      .then(function(result) { return result.text(); })
      .then(function(text) {
        assert_equals(text, content, 'CacheStorage.match() body should match');
      })

     .then(function() {
        return new Response('dummy');
      })
  );
};

self.onmessage = function(e) {
  if (e.data === 'keys') {
    self.caches.open(cache_name)
      .then(function(cache) { return cache.keys(); })
      .then(function(requests) {
        var urls = requests.map(function(request) { return request.url; });
        self.clients.matchAll().then(function(clients) {
          clients.forEach(function(client) {
            client.postMessage(urls);
          });
        });
      });
  }
};
