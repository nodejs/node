var port;

// Exercise the 'onmessage' handler:
self.onmessage = function(e) {
  var message = e.data;
  if ('port' in message) {
    port = message.port;
  }
};

// And an event listener:
self.addEventListener('message', function(e) {
    var message = e.data;
    if ('value' in message) {
      port.postMessage('Acking value: ' + message.value);
    } else if ('done' in message) {
      port.postMessage('quit');
    }
  });
