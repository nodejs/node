self.onmessage = function(e) {
  e.waitUntil(self.clients.matchAll().then(function(clients) {
      clients.forEach(function(client) {
          client.postMessage('Sending message via clients');
          if (!Array.isArray(clients))
            client.postMessage('clients is not an array');
          client.postMessage('quit');
        });
    }));
};
