self.addEventListener('message', function(event) {
    if ('ping' in event.data)
      event.data.ping.postMessage({pong: 'OK'});
  });
