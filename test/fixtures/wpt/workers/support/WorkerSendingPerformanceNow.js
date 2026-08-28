function calcResponse() {
  const response = [
    typeof(workerStart),
    typeof(performance),
    typeof(performance.now),
    performance.now()
  ];
  return response;
}

self.onmessage = function(event) {
  postMessage(calcResponse());
  self.close();
}

self.addEventListener("connect", function(event) {
  const port = event.ports[0];
  port.onmessage = function(event) {
    port.postMessage(calcResponse());
    port.close();
  };
});
