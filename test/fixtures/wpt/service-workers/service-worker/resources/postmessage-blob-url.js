self.onmessage = e => {
  fetch(e.data)
  .then(response => response.text())
  .then(text => e.source.postMessage('Worker reply:' + text));
};
