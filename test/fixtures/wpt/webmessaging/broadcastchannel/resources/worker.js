var c;

function handler(e, reply) {
  if (e.data.ping) {
    c.postMessage(e.data.ping);
    return;
  }
  if (e.data.blob) {
    (() => {
      c.postMessage({blob: new Blob(e.data.blob)});
    })();
    // TODO(https://github.com/web-platform-tests/wpt/issues/7899): Change to
    // some sort of cross-browser GC trigger.
    if (self.gc) self.gc();
  }
  c = new BroadcastChannel(e.data.channel);
  let messages = [];
  c.onmessage = e => {
      if (e.data === 'ready') {
        // Ignore any 'ready' messages from the other thread since there could
        // be some race conditions between this BroadcastChannel instance
        // being created / ready to receive messages and the message being sent.
        return;
      }
      messages.push(e.data);
      if (e.data == 'done')
        reply(messages);
    };
  c.postMessage('from worker');
}

onmessage = e => handler(e, postMessage);

onconnect = e => {
  let port = e.ports[0];
  port.onmessage = e => handler(e, msg => port.postMessage(msg));
};
