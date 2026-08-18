onerror = function(a, b, c, d) {
  return false; // the error is "not handled"
}
function x() {
  y();
}
x();