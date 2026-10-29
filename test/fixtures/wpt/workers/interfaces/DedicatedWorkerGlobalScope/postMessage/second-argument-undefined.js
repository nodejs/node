try {
  postMessage(1, undefined);
} catch(e) {
  postMessage(''+e);
}