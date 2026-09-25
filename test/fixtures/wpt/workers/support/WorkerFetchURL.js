onconnect = e => {
  let port = e.ports[0];
  port.onmessage = (e) => {
    fetch(e.data)
    .then(response => response.text())
    .then(text => port.postMessage('Worker reply:' + text));
  };
}
