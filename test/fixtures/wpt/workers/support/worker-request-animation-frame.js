self.onmessage = function(event) {
  requestAnimationFrame(time => {
    postMessage(time);
    self.close();
  });
}
