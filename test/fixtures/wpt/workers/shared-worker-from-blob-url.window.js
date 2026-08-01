function message_from_port(port) {
  return new Promise(resolve => {
    port.onmessage = e => resolve(e.data);
  });
}

promise_test(async t => {
  const run_result = 'worker_OK_';
  const blob_contents =
    'self.counter = 0; self.onconnect = e => {++self.counter;' +
    'e.source.postMessage("' + run_result + '" + self.counter); };';
  const blob = new Blob([blob_contents]);
  const url = URL.createObjectURL(blob);

  const worker1 = new SharedWorker(url);
  const reply1 = await message_from_port(worker1.port);
  assert_equals(reply1, run_result + '1');
  const worker2 = new SharedWorker(url);
  const reply2 = await message_from_port(worker2.port);
  assert_equals(reply2, run_result + '2');
}, 'Creating a shared worker from a blob URL works.');

promise_test(async t => {
  const run_result = 'worker_OK';
  const blob_contents =
    'self.onconnect = e => { e.source.postMessage("' + run_result + '"); };';
  const blob = new Blob([blob_contents]);
  const url = URL.createObjectURL(blob);

  const worker = new SharedWorker(url);
  URL.revokeObjectURL(url);

  const reply = await message_from_port(worker.port);
  assert_equals(reply, run_result);
}, 'Creating a shared worker from a blob URL works immediately before revoking.');

promise_test(async t => {
  const run_result = 'worker_OK_';
  const blob_contents =
    'self.counter = 0; self.onconnect = e => {++self.counter;' +
    'e.source.postMessage("' + run_result + '" + self.counter); };';
  const blob = new Blob([blob_contents]);
  const url = URL.createObjectURL(blob);

  const worker1 = new SharedWorker(url);
  URL.revokeObjectURL(url);

  const reply1 = await message_from_port(worker1.port);
  assert_equals(reply1, run_result + '1');
  const worker2 = new SharedWorker(url);
  const reply2 = await message_from_port(worker2.port);
  assert_equals(reply2, run_result + '2');
}, 'Connecting to a shared worker on a revoked blob URL works.');

promise_test(async t => {
  const run_result = false;
  const blob_contents = `
let constructedRequest = false;
try {
  new Request("./file.js");
  constructedRequest = true;
} catch (e) {}
self.onconnect = e => {
  e.source.postMessage(constructedRequest);
};
`;
  const blob = new Blob([blob_contents]);
  const url = URL.createObjectURL(blob);

  const worker = new SharedWorker(url);
  const reply = await message_from_port(worker.port);
  assert_equals(reply, run_result, "Should not be able to resolve request with relative file path in blob");
}, 'Blob URLs should not resolve relative to document base URL.');
