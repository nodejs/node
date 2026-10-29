function x() {
  postMessage(1);
  throw new Error();
}
setTimeout(x, 0);
close();
setTimeout(x, 0);