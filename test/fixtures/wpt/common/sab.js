const createBuffer = (() => {
  // See https://github.com/whatwg/html/issues/5380 for why not `new SharedArrayBuffer()`
  const sabConstructor = new WebAssembly.Memory({ shared:true, initial:0, maximum:0 }).buffer.constructor;
  return (type, length) => {
    if (type === "ArrayBuffer") {
      return new ArrayBuffer(length);
    } else if (type === "SharedArrayBuffer") {
      if (sabConstructor.name !== "SharedArrayBuffer") {
        throw new Error("WebAssembly.Memory does not support shared:true");
      }
      return new sabConstructor(length);
    } else {
      throw new Error("type has to be ArrayBuffer or SharedArrayBuffer");
    }
  }
})();
