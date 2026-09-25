importScripts('./extendable-message-event-utils.js');

self.addEventListener('message', function(event) {
    switch (event.data.type) {
      case 'start':
        // Send a ping message to another service worker.
        self.registration.waiting.postMessage(
            {type: 'ping', client_id: event.source.id});
        break;
      case 'pong':
        var results = {
            pingOrPong: 'pong',
            event: ExtendableMessageEventUtils.serialize(event)
        };
        var client_id = event.data.client_id;
        event.waitUntil(clients.get(client_id)
            .then(function(client) {
                client.postMessage({type: 'record', results: results});
                client.postMessage({type: 'finish'});
              }));
        break;
    }
  });
