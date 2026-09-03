var requests = [];

self.addEventListener('message', function(event) {
    event.waitUntil(self.clients.matchAll()
      .then(function(clients) {
          var client_urls = [];
          for(var client of clients){
            client_urls.push(client.url);
          }
          client_urls = client_urls.sort();
          event.data.port.postMessage(
              {clients: client_urls, requests: requests});
          requests = [];
        }));
  });

self.addEventListener('fetch', function(event) {
    requests.push(event.request.url);
    event.respondWith(fetch(event.request));
  });
