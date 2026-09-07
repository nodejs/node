try {
  postMessage(false, [null]);
} catch(e) {
  postMessage(e instanceof TypeError);
}