// The error's `message` values in Worker error event handlers are tested.
// While not explicitly specified in the HTML spec, we expect some information
// about thrown errors (e.g. original message, the string "TypeError", etc.)
// to appear in the `message`.

function prepareHandler(t, error, expectedCount) {
  let count = 0;
  return t.step_func(e => {
      e.preventDefault();

      assert_regexp_match(
          e.message,
          /Throw in/,
          'e.message should contain the message of the thrown error');

      if (error === 'DOMException-TypeError') {
        assert_regexp_match(e.message, /TypeError/);
      }

      ++count;
      if (count >= expectedCount) {
        t.done();
      }
    });
}

function expectErrors(worker, error, expectedCount, title) {
  async_test(t => {
      worker.addEventListener('error',
                              prepareHandler(t, error, expectedCount));
    }, title+ ': listener');
  async_test(t => {
      worker.onerror = prepareHandler(t, error, expectedCount);
    }, title + ': handler');
}

function runTest(type, error) {
  for (const location of ['toplevel',
                          'setTimeout-function',
                          'setTimeout-string',
                          'onmessage',
                          'onerror']) {
    const worker = new Worker(
        'throw.js?throw-in-' + location + '&error=' + error,
        {type});
    let expectedCount = 1;
    if (location === 'onmessage') {
      // This makes the worker's message handler to throw an error.
      worker.postMessage('foo');
    }
    if (location === 'onerror') {
      // This makes the worker's message handler to throw an error,
      // AND worker's error handler to throw another error.
      // Therefore we expect two errors here.
      worker.postMessage('foo');
      expectedCount = 2;
    }
    expectErrors(worker, error, expectedCount,
        'Throw ' + error + ' in ' + location + ': ' + type);
  }
}
