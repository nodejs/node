structuredCloneBatteryOfTests.push({
  description: 'ArrayBuffer',
  async f(runner) {
    const buffer = new Uint8Array([1]).buffer;
    const copy = await runner.structuredClone(buffer, [buffer]);
    assert_equals(buffer.byteLength, 0);
    assert_equals(copy.byteLength, 1);
  }
});

structuredCloneBatteryOfTests.push({
  description: 'MessagePort',
  async f(runner) {
    const {port1, port2} = new MessageChannel();
    const copy = await runner.structuredClone(port2, [port2]);
    const msg = new Promise(resolve => port1.onmessage = resolve);
    copy.postMessage('ohai');
    assert_equals((await msg).data, 'ohai');
  }
});

// TODO: ImageBitmap
