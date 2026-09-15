onconnect = e => {
  e.ports[0].postMessage(self.name);
};
