var eventsSeen = [];

function handler(event) { eventsSeen.push(event.type); }

['activate', 'install'].forEach(function(type) {
    self.addEventListener(type, handler);
  });

onmessage = function(e) {
  var message = e.data;
  message.port.postMessage({events: eventsSeen});
};
