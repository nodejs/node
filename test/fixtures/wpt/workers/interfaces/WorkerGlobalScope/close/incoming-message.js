onmessage = function(e) {
  postMessage(1);
  throw new Error();
}
close();