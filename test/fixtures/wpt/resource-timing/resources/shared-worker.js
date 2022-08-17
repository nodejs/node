self.onconnect = e => {
  e.ports[0].postMessage(performance.timeOrigin);
}