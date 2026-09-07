onmessage = function(e) {
  postMessage(e.ports instanceof Array && e.ports.length === 0);
}