// META: script=/workers/modules/resources/import-test-cases.js

// Imports |testCase.scriptURL| on a shared worker loaded from a blob URL,
// and waits until the list of imported modules is sent from the worker. Passes
// if the list is equal to |testCase.expectation|.
function import_blob_url_test(testCase) {
  promise_test(async () => {
    const importURL = new URL(testCase.scriptURL, location.href);
    const blob =
        new Blob([`import "${importURL}";`], {type: 'text/javascript'});
    const blobURL = URL.createObjectURL(blob);
    const worker = new SharedWorker(blobURL, {type: 'module'});
    worker.port.postMessage('Send message for tests from main script.');
    const msgEvent = await new Promise((resolve, reject) => {
      worker.port.onmessage = resolve;
      worker.onerror = error => {
        const msg = error instanceof ErrorEvent ? error.message
                                                : 'unknown error';
        reject(msg);
      };
    });
    assert_array_equals(msgEvent.data, testCase.expectation);
  }, testCase.description);
}

testCases.forEach(import_blob_url_test);
