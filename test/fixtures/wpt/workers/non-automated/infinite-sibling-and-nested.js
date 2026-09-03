function createWorker() {
  var worker = new Worker('infinite-nested.js?' + Math.random());
  worker.onmessage = function(e) {
    postMessage(e.data);
    createWorker();
  }
}
createWorker();