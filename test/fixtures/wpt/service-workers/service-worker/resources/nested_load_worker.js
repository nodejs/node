// Entry point for dedicated workers.
self.onmessage = evt => {
  try {
    const worker = new Worker('load_worker.js');
    worker.onmessage = msg => self.postMessage(msg.data);
    worker.postMessage(evt.data);
  } catch (err) {
    self.postMessage('Unexpected error! ' + err.message);
  }
};

// Entry point for shared workers.
self.onconnect = evt => {
  evt.ports[0].onmessage = e => {
    try {
      const worker = new Worker('load_worker.js');
      worker.onmessage = msg => evt.ports[0].postMessage(msg.data);
      worker.postMessage(e.data);
    } catch (err) {
      evt.ports[0].postMessage('Unexpected error! ' + err.message);
    }
  };
};
