onmessage = function(e) {
  var message = e.data;
  if (typeof message === 'object' && 'port' in message) {
    var response = 'Ack for: ' + message.from;
    message.port.postMessage(response);
  }
};
