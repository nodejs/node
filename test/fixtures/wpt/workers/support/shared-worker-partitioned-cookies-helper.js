// Return a generator containing the worker's message.
//
// Usage:
//   const worker = new SharedWorker(...);
//   const nextMessage = worker_message_generator(worker);
//   const msg_1 = await nextMessage();
//   const msg_2 = await nextMessage();
//   const msg_3 = await nextMessage();
function worker_message_generator(shared_worker) {
  const buffer = [];
  let resolve = null;

  shared_worker.port.onmessage = message => {
    buffer.push(message.data);
    if (resolve) {
      resolve();
    }
  }
  shared_worker.port.start();

  return async function() {
    if (buffer.length != 0) {
      return buffer.shift();
    }
    await new Promise(r => resolve = r);
    return buffer.shift();
  }
}
