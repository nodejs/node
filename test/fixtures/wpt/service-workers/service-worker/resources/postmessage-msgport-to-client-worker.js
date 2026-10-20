self.onmessage = function(e) {
  e.waitUntil(self.clients.matchAll().then(function(clients) {
      clients.forEach(function(client) {
          var messageChannel = new MessageChannel();
          messageChannel.port1.onmessage =
            onMessageViaMessagePort.bind(null, messageChannel.port1);
          client.postMessage(undefined, [messageChannel.port2]);
        });
    }));
};

function onMessageViaMessagePort(port, e) {
  var message = e.data;
  if ('value' in message) {
    port.postMessage({ack: 'Acking value: ' + message.value});
  } else if ('done' in message) {
    port.postMessage({done: true});
  }
}
