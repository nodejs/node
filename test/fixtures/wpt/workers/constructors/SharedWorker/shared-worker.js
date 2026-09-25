onconnect = e => {
  const port = e.ports[0];
  port.onmessage = e => {
    port.postMessage('ping');
  }
}
