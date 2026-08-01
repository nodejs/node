onerror = function(a, b, c, d) {
  postMessage([a, b, c, d]);
  return true; // the error is "handled"
}
function x() {
  y();
}
x();