self.addEventListener('message', function(event) {
    if ('port' in event.data) {
      var port = event.data.port;

      var channel = new MessageChannel();
      channel.port1.onmessage = function(event) {
        if ('pong' in event.data)
          port.postMessage(event.data.pong);
      };
      self.registration.active.postMessage({ping: channel.port2},
                                           [channel.port2]);
    } else if ('ping' in event.data) {
      event.data.ping.postMessage({pong: 'OK'});
    }
  });
