const createBuffer = (() => {
  // See https://github.com/whatwg/html/issues/5380 for why not `new SharedArrayBuffer()`
  let sabConstructor;
  try {
    sabConstructor = new WebAssembly.Memory({ shared:true, initial:0, maximum:0 }).buffer.constructor;
  } catch(e) {
    sabConstructor = null;
  }
  return (type, length) => {
    if (type === "ArrayBuffer") {
      return new ArrayBuffer(length);
    } else if (type === "SharedArrayBuffer") {
      if (sabConstructor && sabConstructor.name !== "SharedArrayBuffer") {
        throw new Error("WebAssembly.Memory does not support shared:true");
      }
      return new sabConstructor(length);
    } else {
      throw new Error("type has to be ArrayBuffer or SharedArrayBuffer");
    }
  }
})();
