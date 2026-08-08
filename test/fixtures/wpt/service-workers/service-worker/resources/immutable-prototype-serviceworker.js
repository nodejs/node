function prototypeChain(global) {
  let result = [];
  while (global !== null) {
    let thrown = false;
    let next = Object.getPrototypeOf(global);
    try {
      Object.setPrototypeOf(global, {});
      result.push('mutable');
    } catch (e) {
      result.push('immutable');
    }
    global = next;
  }
  return result;
}

self.onmessage = function(e) {
  e.data.postMessage(prototypeChain(self));
};
