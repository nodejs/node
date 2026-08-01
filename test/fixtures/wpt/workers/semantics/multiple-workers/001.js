if ('onmessage' in self) { // dedicated worker
  onmessage = function(e) {
    postMessage(e.data);
  }
} else { // shared worker
  onconnect = function(e) {
    e.ports[0].onmessage = function(e) {
      this.postMessage(e.data);
    }
  }
}