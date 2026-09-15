importScripts('./extendable-message-event-utils.js');

self.addEventListener('message', function(event) {
    switch (event.data.type) {
      case 'start':
        self.registration.active.postMessage(
            {type: '1st', client_id: event.source.id});
        break;
      case '1st':
        // 1st loopback message via ServiceWorkerRegistration.active.
        var results = {
            trial: 1,
            event: ExtendableMessageEventUtils.serialize(event)
        };
        var client_id = event.data.client_id;
        event.source.postMessage({type: '2nd', client_id: client_id});
        event.waitUntil(clients.get(client_id)
            .then(function(client) {
                client.postMessage({type: 'record', results: results});
              }));
        break;
      case '2nd':
        // 2nd loopback message via ExtendableMessageEvent.source.
        var results = {
            trial: 2,
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
