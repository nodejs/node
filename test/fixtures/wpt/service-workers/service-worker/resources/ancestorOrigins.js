self.onmessage = (evt) => {
  evt.source.postMessage({
    ancestorOrigins: evt.source.ancestorOrigins,
    sameObject: evt.source.ancestorOrigins === evt.source.ancestorOrigins
  });
};
