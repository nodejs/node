// META: global=window,worker
// META: script=/common/sab.js

["ArrayBuffer", "SharedArrayBuffer"].forEach(arrayBufferOrSharedArrayBuffer => {
  test(() => {
    const buf = createBuffer(arrayBufferOrSharedArrayBuffer, 2);
    const view = new Uint8Array(buf);
    const buf2 = createBuffer(arrayBufferOrSharedArrayBuffer, 2);
    const view2 = new Uint8Array(buf2);
    const decoder = new TextDecoder("utf-8");
    view[0] = 0xEF;
    view[1] = 0xBB;
    view2[0] = 0xBF;
    view2[1] = 0x40;
    assert_equals(decoder.decode(buf, {stream:true}), "");
    view[0] = 0x01;
    view[1] = 0x02;
    assert_equals(decoder.decode(buf2), "@");
  }, "Modify buffer after passing it in (" + arrayBufferOrSharedArrayBuffer  + ")");
});
