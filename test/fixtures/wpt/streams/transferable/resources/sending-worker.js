'use strict';
importScripts('helpers.js');

const orig = createOriginalReadableStream();
postMessage(orig, [orig]);
