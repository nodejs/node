self.onmessage = function(e) {
  var port = e.data.port;
  var message = [];

  var promise = Promise.resolve()
    .then(function() {
        // 1st matchAll()
        return self.clients.matchAll().then(function(clients) {
            clients.forEach(function(client) {
                message.push(client.id);
              });
          });
      })
    .then(function() {
        // 2nd matchAll()
        return self.clients.matchAll().then(function(clients) {
            clients.forEach(function(client) {
                message.push(client.id);
              });
          });
      })
    .then(function() {
        // Send an array containing ids of clients from 1st and 2nd matchAll()
        port.postMessage(message);
      });
  e.waitUntil(promise);
};
