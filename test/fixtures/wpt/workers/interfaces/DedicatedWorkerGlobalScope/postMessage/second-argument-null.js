try {
  postMessage(1, null);
} catch(e) {
  postMessage(e instanceof TypeError);
}