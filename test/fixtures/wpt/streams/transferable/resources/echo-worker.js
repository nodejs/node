// A worker that just transfers back any message that is sent to it.
onmessage = evt => postMessage(evt.data, [evt.data]);
