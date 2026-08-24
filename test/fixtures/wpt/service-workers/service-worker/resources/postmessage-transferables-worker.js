var messageHandler = function(port, e) {
    var text_decoder = new TextDecoder;
    port.postMessage({
      content: text_decoder.decode(e.data),
      byteLength: e.data.byteLength
    });

    // Send back the array buffer via Client.postMessage.
    port.postMessage(e.data, [e.data.buffer]);

    port.postMessage({
      content: text_decoder.decode(e.data),
      byteLength: e.data.byteLength
    });
};

self.addEventListener('message', e => {
    if (e.ports[0]) {
      // Wait for messages sent via MessagePort.
      e.ports[0].onmessage = messageHandler.bind(null, e.ports[0]);
      return;
    }
    messageHandler(e.source, e);
  });
