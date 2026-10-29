// This worker dynamically imports the script URL sent by postMessage(), and
// sends back an error name if the dynamic import fails.
if ('DedicatedWorkerGlobalScope' in self &&
    self instanceof DedicatedWorkerGlobalScope) {
  self.onmessage = msg_event => {
    import(msg_event.data)
        .then(module => postMessage(module.meta_url))
        .catch(e => postMessage(e.name));
  };
} else if (
    'SharedWorkerGlobalScope' in self &&
    self instanceof SharedWorkerGlobalScope) {
  self.onconnect = connect_event => {
    const port = connect_event.ports[0];
    port.onmessage = msg_event => {
      import(msg_event.data)
          .then(module => port.postMessage(module.meta_url))
          .catch(e => port.postMessage(e.name));
    };
  };
}
