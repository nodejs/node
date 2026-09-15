addEventListener('message', evt => {
  evt.source.postMessage(evt.data);
});
