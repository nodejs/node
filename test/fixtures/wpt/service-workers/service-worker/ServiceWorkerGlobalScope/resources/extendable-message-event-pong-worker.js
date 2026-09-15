importScripts('./extendable-message-event-utils.js');

self.addEventListener('message', function(event) {
    switch (event.data.type) {
      case 'ping':
        var results = {
            pingOrPong: 'ping',
            event: ExtendableMessageEventUtils.serialize(event)
        };
        var client_id = event.data.client_id;
        event.waitUntil(clients.get(client_id)
            .then(function(client) {
                client.postMessage({type: 'record', results: results});
                event.source.postMessage({type: 'pong', client_id: client_id});
              }));
        break;
    }
  });
