// Text*Stream should still work even if the realm is detached.

// Adds an iframe to the document and returns it.
function addIframe() {
  const iframe = document.createElement('iframe');
  document.body.appendChild(iframe);
  return iframe;
}

promise_test(async t => {
  const iframe = addIframe();
  const stream = new iframe.contentWindow.TextDecoderStream();
  const readPromise = stream.readable.getReader().read();
  const writer = stream.writable.getWriter();
  await writer.ready;
  iframe.remove();
  return Promise.all([writer.write(new Uint8Array([65])),readPromise]);
}, 'TextDecoderStream: write in detached realm should succeed');

promise_test(async t => {
  const iframe = addIframe();
  const stream = new iframe.contentWindow.TextEncoderStream();
  const readPromise = stream.readable.getReader().read();
  const writer = stream.writable.getWriter();
  await writer.ready;
  iframe.remove();
  return Promise.all([writer.write('A'), readPromise]);
}, 'TextEncoderStream: write in detached realm should succeed');

for (const type of ['TextEncoderStream', 'TextDecoderStream']) {
  promise_test(async t => {
    const iframe = addIframe();
    const stream = new iframe.contentWindow[type]();
    iframe.remove();
    return stream.writable.close();
  }, `${type}: close in detached realm should succeed`);
}
