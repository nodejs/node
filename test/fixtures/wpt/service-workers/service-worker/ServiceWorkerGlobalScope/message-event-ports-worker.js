self.onmessage = e => {
    e.source.postMessage(e.ports === e.ports ? "same ports array" : "different ports array");
};
