// TransformStream should still work even if the realm is detached.

// Adds an iframe to the document and returns it.
function addIframe() {
  const iframe = document.createElement('iframe');
  document.body.appendChild(iframe);
  return iframe;
}

promise_test(async t => {
  const iframe = addIframe();
  const stream = new iframe.contentWindow.TransformStream();
  const readPromise = stream.readable.getReader().read();
  const writer = stream.writable.getWriter();
  iframe.remove();
  return Promise.all([writer.write('A'), readPromise]);
}, 'TransformStream: write in detached realm should succeed');
