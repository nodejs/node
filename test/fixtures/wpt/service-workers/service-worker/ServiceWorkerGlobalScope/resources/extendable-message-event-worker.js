importScripts('./extendable-message-event-utils.js');

self.addEventListener('message', function(event) {
    event.source.postMessage(ExtendableMessageEventUtils.serialize(event));
  });
