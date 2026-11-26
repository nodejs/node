// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

function startGrowWorker(delta) {
  function workerCode(delta) {
    onmessage = function({data: memory}) {
      memory.grow(delta);
      postMessage('grown');
    };
  }
  return new Worker(workerCode, {type: 'function', arguments: [delta]});
}

(function TestMemoryBufferRefreshedWithoutStackCheck() {
  print(arguments.callee.name);
  const memory = new WebAssembly.Memory({initial: 1, maximum: 5, shared: true});
  const oldBuf = memory.buffer;
  assertEquals(kPageSize, oldBuf.byteLength);

  const worker = startGrowWorker(1);
  worker.postMessage(memory);
  const msg = worker.getMessage();
  // Read memory.buffer immediately without any intervening JS call or loop
  // back-edge that would process the GROW_SHARED_MEMORY stack-guard interrupt.
  const newBuf = memory.buffer;

  assertEquals('grown', msg);
  assertNotSame(oldBuf, newBuf);
  assertEquals(kPageSize, oldBuf.byteLength);
  assertEquals(2 * kPageSize, newBuf.byteLength);
  assertEquals(2 * kPageSize, newBuf.maxByteLength);
  assertTrue(Object.isFrozen(newBuf));
  worker.terminate();
})();

(function TestToFixedLengthBufferRefreshedWithoutStackCheck() {
  print(arguments.callee.name);
  const memory = new WebAssembly.Memory({initial: 1, maximum: 5, shared: true});
  const oldBuf = memory.toFixedLengthBuffer();
  assertEquals(kPageSize, oldBuf.byteLength);

  const worker = startGrowWorker(1);
  worker.postMessage(memory);
  const msg = worker.getMessage();
  // Call toFixedLengthBuffer() directly before any stack-guard interrupt runs.
  const newBuf = memory.toFixedLengthBuffer();

  assertEquals('grown', msg);
  assertNotSame(oldBuf, newBuf);
  assertEquals(kPageSize, oldBuf.byteLength);
  assertEquals(2 * kPageSize, newBuf.byteLength);
  assertEquals(2 * kPageSize, newBuf.maxByteLength);
  assertTrue(Object.isFrozen(newBuf));
  worker.terminate();
})();

(function TestMemoryBufferRefreshedAfterWasmGrowWithoutStackCheck() {
  print(arguments.callee.name);
  const builder = new WasmModuleBuilder();
  builder.addImportedMemory('env', 'memory', 1, 5, true);
  builder.addFunction('grow', kSig_i_i)
      .addBody([kExprLocalGet, 0, kExprMemoryGrow, 0])
      .exportFunc();
  builder.addFunction('size', kSig_i_v)
      .addBody([kExprMemorySize, 0])
      .exportFunc();
  const module = new WebAssembly.Module(builder.toBuffer());

  function workerCode() {
    onmessage = function({data: {module, memory}}) {
      const instance = new WebAssembly.Instance(module, {env: {memory}});
      instance.exports.grow(2);
      postMessage('grown');
    };
  }

  const memory = new WebAssembly.Memory({initial: 1, maximum: 5, shared: true});
  const instance = new WebAssembly.Instance(module, {env: {memory}});
  const oldBuf = memory.buffer;
  assertEquals(kPageSize, oldBuf.byteLength);

  const worker = new Worker(workerCode, {type: 'function'});
  worker.postMessage({module, memory});
  const msg = worker.getMessage();
  // Access memory.buffer immediately before any stack-guard interrupt runs.
  const newBuf = memory.buffer;
  const pages = instance.exports.size();

  assertEquals('grown', msg);
  assertEquals(3, pages);
  assertNotSame(oldBuf, newBuf);
  assertEquals(3 * kPageSize, newBuf.byteLength);
  assertEquals(3 * kPageSize, newBuf.maxByteLength);
  assertTrue(Object.isFrozen(newBuf));
  worker.terminate();
})();

(function TestGrowZeroKeepsSameBufferWithoutStackCheck() {
  print(arguments.callee.name);
  const memory = new WebAssembly.Memory({initial: 1, maximum: 5, shared: true});
  const oldBuf = memory.buffer;

  const worker = startGrowWorker(0);
  worker.postMessage(memory);
  const msg = worker.getMessage();
  const afterBuf = memory.buffer;
  const afterFixed = memory.toFixedLengthBuffer();

  assertEquals('grown', msg);
  assertSame(oldBuf, afterBuf);
  assertSame(oldBuf, afterFixed);
  assertEquals(kPageSize, afterBuf.byteLength);
  worker.terminate();
})();

(function TestResizableBufferNotReplacedWithoutStackCheck() {
  print(arguments.callee.name);
  const memory = new WebAssembly.Memory({initial: 1, maximum: 5, shared: true});
  const gsab = memory.toResizableBuffer();
  assertTrue(gsab.growable);
  assertEquals(kPageSize, gsab.byteLength);

  const worker = startGrowWorker(1);
  worker.postMessage(memory);
  const msg = worker.getMessage();
  const afterBuf = memory.buffer;
  const afterResizable = memory.toResizableBuffer();

  assertEquals('grown', msg);
  assertSame(gsab, afterBuf);
  assertSame(gsab, afterResizable);
  assertEquals(2 * kPageSize, afterBuf.byteLength);
  worker.terminate();
})();
