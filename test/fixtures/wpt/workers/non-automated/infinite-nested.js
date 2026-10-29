postMessage(1);
var w = new Worker('infinite-nested.js');
w.onmessage = function(e) {
  postMessage(e.data);
}