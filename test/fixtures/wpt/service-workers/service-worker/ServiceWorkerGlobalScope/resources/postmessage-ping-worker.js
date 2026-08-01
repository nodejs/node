self.addEventListener('message', function(event) {
    if ('port' in event.data) {
      var port = event.data.port;

      var channel = new MessageChannel();
      channel.port1.onmessage = function(event) {
        if ('pong' in event.data)
          port.postMessage(event.data.pong);
      };

      // Send a ping message to another service worker.
      self.registration.waiting.postMessage({ping: channel.port2},
                                            [channel.port2]);
    }
  });
