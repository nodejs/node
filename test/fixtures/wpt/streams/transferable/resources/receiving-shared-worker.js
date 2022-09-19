'use strict';
importScripts('/resources/testharness.js', 'helpers.js');

onconnect = evt => {
  const port = evt.source;
  const promise = testMessageEvent(port);
  port.start();
  promise
      .then(() => port.postMessage('OK'))
      .catch(err => port.postMessage(`BAD: ${err}`));
};
