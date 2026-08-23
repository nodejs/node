importScripts('test-helpers.sub.js');

var page_url = normalizeURL('../clients-matchall-on-evaluation.https.html');

self.clients.matchAll({includeUncontrolled: true})
  .then(function(clients) {
      clients.forEach(function(client) {
          if (client.url == page_url)
            client.postMessage('matched');
        });
    });
