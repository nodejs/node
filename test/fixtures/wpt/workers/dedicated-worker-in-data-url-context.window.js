// META: title=data URL dedicated worker in data URL context
// META: script=/service-workers/service-worker/resources/test-helpers.sub.js
const mimeType = 'application/javascript';

// Tests creating a dedicated worker in a data URL iframe.
promise_test(async t => {
  const nestedWorkerScriptURL =
      new URL('/workers/support/post-message-on-load-worker.js', location.href);

  // This code will be executed in a data URL iframe. The iframe tries to create
  // a dedicated worker from |nestedWorkerScriptURL|, but that should result in
  // a failure. This is because the data URL iframe has an opaque origin, and
  // script fetch is handled as a cross-origin request.
  const frameCode = `
      <script>
      try {
        const worker = new Worker('${nestedWorkerScriptURL}');
        worker.onmessage = e => {
          window.parent.postMessage(
              'Worker construction unexpectedly succeeded', '*');
        };
        worker.onerror = e => window.parent.postMessage('PASS', '*');
      } catch (e) {
        // Cross-origin request should asynchronously fail during worker script
        // fetch because its request mode is 'same-origin'.
        window.parent.postMessage(
            'Worker construction unexpectedly synchronously failed', '*');
      }
      </script>
  `;

  const p = new Promise(r => window.onmessage = e => r(e.data));
  const frame = await with_iframe(`data:text/html;base64,${btoa(frameCode)}`);
  const result = await p;
  assert_equals(result, 'PASS');
}, 'Create a dedicated worker in a data url frame');

// Tests creating a dedicated worker in a data URL dedicated worker (i.e.,
// nested dedicated worker).
promise_test(async t => {
  const nestedWorkerScriptURL =
      new URL('/workers/support/post-message-on-load-worker.js', location.href);

  // This code will be executed in a data URL dedicated worker. The worker tries
  // to create a nested dedicated worker from |nestedWorkerScriptURL|, but that
  // should result in a failure. This is because the data URL dedicated worker
  // has an opaque origin, and script fetch is handled as a cross-origin
  // request.
  const workerCode = `
      try {
        const worker = new Worker('${nestedWorkerScriptURL}');
        worker.onmessage =
            e => postMessage('Worker construction unexpectedly succeeded');
        worker.onerror = e => postMessage('PASS');
      } catch (e) {
        // Cross-origin request should asynchronously fail during worker script
        // fetch because its request mode is 'same-origin'.
        postMessage('Worker construction unexpectedly synchronously failed');
      }
  `;

  const result = await new Promise((resolve, reject) => {
    const worker = new Worker(`data:${mimeType};base64,${btoa(workerCode)}`);
    worker.onmessage = e => resolve(e.data);
    worker.onerror = e => reject(e.message);
  });
  assert_equals(result, 'PASS');
}, 'Create a dedicated worker in a data url dedicated worker');

// Tests creating a data URL dedicated worker in a data URL iframe.
promise_test(async t => {
  // This code will be executed in a data URL iframe. The iframe tries to create
  // a data URL dedicated worker. Fetching a data URL from the data URL iframe
  // whose origin is opaque is allowed, so the worker construction should
  // succeed. The iframe posts the result to the parent frame.
  const frameCode = `
      <script>
      const worker = new Worker('data:${mimeType},postMessage("PASS");');
      worker.onmessage = e => window.parent.postMessage(e.data, '*');
      worker.onerror = e => {
        window.parent.postMessage('FAIL: ' + e.message, '*');
      };
      </script>
  `;

  const p = new Promise(r => window.onmessage = e => r(e.data));
  const frame = await with_iframe(`data:text/html;base64,${btoa(frameCode)}`);
  const result = await p;
  assert_equals(result, 'PASS');
}, 'Create a data url dedicated worker in a data url frame');

// Tests creating a data URL dedicated worker in a data URL dedicated worker
// (i.e., nested dedicated worker).
promise_test(async t => {
  // This code will be executed in a data URL dedicated worker. The worker tries
  // to create a nested data URL dedicated worker. Fetching a data URL from the
  // data URL dedicated worker is allowed, so the worker construction should
  // succeed. The worker posts the result to the parent frame.
  const workerCode = `
      const worker = new Worker('data:${mimeType},postMessage("PASS");');
      worker.onmessage = e => postMessage(e.data);
      worker.onerror = e => postMessage('FAIL: ' + e.message);
  `;

  const result = await new Promise((resolve, reject) => {
    const worker = new Worker(`data:${mimeType};base64,${btoa(workerCode)}`);
    worker.onmessage = e => resolve(e.data);
    worker.onerror = e => reject(e.message);
  });
  assert_equals(result, 'PASS');
}, 'Create a data url dedicated worker in a data url dedicated worker');
