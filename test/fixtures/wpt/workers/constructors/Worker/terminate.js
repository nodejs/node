(function f() {
  postMessage(1);
  setTimeout(f, 0);
})();