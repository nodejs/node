'use strict';
importScripts('/resources/testharness.js', 'helpers.js');

onmessage = msg => {
  const client = msg.source;
  if (msg.data === 'SEND') {
    sendingTest(client);
  } else {
    receivingTest(msg, client);
  }
};

function sendingTest(client) {
  const orig = createOriginalReadableStream();
  try {
    client.postMessage(orig, [orig]);
  } catch (e) {
    client.postMessage(e.message);
  }
}

function receivingTest(msg, client) {
  try {
    msg.waitUntil(testMessage(msg)
                  .then(() => client.postMessage('OK'))
                  .catch(e => client.postMessage(`BAD: ${e}`)));
  } catch (e) {
    client.postMessage(`BAD: ${e}`);
  }
}
