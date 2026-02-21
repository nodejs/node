'use strict';
importScripts('helpers.js');

onconnect = msg => {
  const port = msg.source;
  const orig = createOriginalReadableStream();
  try {
    port.postMessage(orig, [orig]);
  } catch (e) {
    port.postMessage(e.message);
  }
};
