'use strict';
const {
  JSONParse,
  JSONStringify,
} = primordials;
const {
  emitProtocolResponseInParent,
  addIoData,
} = internalBinding('inspector');
const { AbortController } = require('internal/abort_controller');
const { setTimeout, clearTimeout } = require('timers');
const { fetch } = require('internal/deps/undici/undici');

module.exports = function setupInspectorWorker(_, port) {
  function emitResponse(callId, sessionId, success, streamId = null) {
    const result = {
      id: callId,
      result: {
        resource: {
          success,
        },
      },
    };
    if (success && streamId !== null) {
      result.result.resource.stream = streamId.toString();
    }
    emitProtocolResponseInParent(callId, JSONStringify(result), sessionId);
  }

  port.on('message', (msg) => {
    const { sessionId, callId, message } = msg;
    const url = JSONParse(message)?.params?.url;
    if (!url) return;

    const controller = new AbortController();
    const timeoutId = setTimeout(() => controller.abort(), 2000);

    fetch(url, { signal: controller.signal })
      .then((response) => {
        clearTimeout(timeoutId);
        if (!response.ok) {
          emitResponse(callId, sessionId, false);
          return null;
        }
        return response.text().then((text) => {
          const streamId = addIoData(text);
          emitResponse(callId, sessionId, true, streamId);
        });
      })
      .catch(() => {
        clearTimeout(timeoutId);
        emitResponse(callId, sessionId, false);
      });
  });
};
