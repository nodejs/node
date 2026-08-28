// META: script=/workers/modules/resources/import-test-cases.js

// Starts a dedicated worker for |testCase.scriptURL| and waits until the list
// of imported modules is sent from the worker. Passes if the list is equal to
// |testCase.expectation|.
function import_test(testCase) {
  promise_test(async () => {
    const worker = new Worker(testCase.scriptURL, { type: 'module' });
    worker.postMessage('Send message for tests from main script.');
    const msgEvent = await new Promise((resolve, reject) => {
      worker.onmessage = resolve;
      worker.onerror = error => {
        const msg = error instanceof ErrorEvent ? error.message
                                                : 'unknown error';
        reject(msg);
      };
    });
    assert_array_equals(msgEvent.data, testCase.expectation);
  }, testCase.description);
}

testCases.forEach(import_test);
