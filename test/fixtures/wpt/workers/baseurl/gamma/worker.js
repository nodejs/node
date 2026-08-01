var worker = new Worker("subworker.js");
worker.onmessage = function(e) {
  postMessage(e.data);
}
