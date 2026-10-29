self.addEventListener('message', event => {
  event.source.postMessage(event.data);
});
