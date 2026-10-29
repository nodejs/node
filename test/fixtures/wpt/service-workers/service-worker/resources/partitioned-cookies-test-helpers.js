// Return a generator containing the worker's message.
//
// Usage:
//   await navigator.serviceWorker.register(...)
//   ...
//   const nextMessage = worker_message_generator();
//   const msg_1 = await nextMessage();
//   const msg_2 = await nextMessage();
//   const msg_3 = await nextMessage();
//
// Worker should have its own onmessage event listener that
// postMessages replies to the DOM.
function worker_message_generator() {
  const buffer = [];
  let resolve = null;

  navigator.serviceWorker.addEventListener('message', message => {
    buffer.push(message.data);
    if (resolve) {
      resolve();
    }
  });

  return async function () {
    if (buffer.length != 0) {
      return buffer.shift();
    }
    await new Promise(r => resolve = r);
    return buffer.shift();
  }
}
