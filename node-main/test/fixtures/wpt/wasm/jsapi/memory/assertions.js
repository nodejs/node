function assert_ArrayBuffer(actual, { size=0, shared=false, detached=false }, message) {
  // https://github.com/WebAssembly/spec/issues/840
  // See https://github.com/whatwg/html/issues/5380 for why not `self.SharedArrayBuffer`
  const isShared = !("isView" in actual.constructor);
  assert_equals(isShared, shared, `${message}: constructor`);
  const sharedString = shared ? "Shared" : "";
  assert_equals(actual.toString(), `[object ${sharedString}ArrayBuffer]`, `${message}: toString()`);
  assert_equals(Object.getPrototypeOf(actual).toString(), `[object ${sharedString}ArrayBuffer]`, `${message}: prototype toString()`);
  if (detached) {
    // https://github.com/tc39/ecma262/issues/678
    let byteLength;
    try {
      byteLength = actual.byteLength;
    } catch (e) {
      byteLength = 0;
    }
    assert_equals(byteLength, 0, `${message}: detached size`);
  } else {
    assert_equals(actual.byteLength, 0x10000 * size, `${message}: size`);
    if (size > 0) {
      const array = new Uint8Array(actual);
      assert_equals(array[0], 0, `${message}: first element`);
      assert_equals(array[array.byteLength - 1], 0, `${message}: last element`);
    }
  }
  assert_equals(Object.isFrozen(actual), shared, "buffer frozen");
  assert_equals(Object.isExtensible(actual), !shared, "buffer extensibility");
}

function assert_Memory(memory, { size=0, shared=false }) {
  assert_equals(Object.getPrototypeOf(memory), WebAssembly.Memory.prototype,
                "prototype");
  assert_true(Object.isExtensible(memory), "extensible");

  // https://github.com/WebAssembly/spec/issues/840
  assert_equals(memory.buffer, memory.buffer, "buffer should be idempotent");
  assert_ArrayBuffer(memory.buffer, { size, shared });
}
