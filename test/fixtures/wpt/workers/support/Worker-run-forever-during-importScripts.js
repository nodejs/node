importScripts('Worker-run-forever.js');

// This is not expected to run.
postMessage('after importScripts()');
