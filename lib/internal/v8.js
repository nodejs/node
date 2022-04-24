'use strict';

let streamingHandler;
function setWasmStreamingHandler(handler) {
  if (handler !== undefined && handler !== null) {
    validateObject(handler, 'handler');
    validateFunction(handler.get, 'handler.get');
    validateFunction(handler.set, 'handler.set');
    validateFunction(handler.delete, 'handler.delete');
  }
  streamingHandler = handler;
}

function getWasmStreamingHandler() {
  return streamingHandler;
}

module.exports = {
  setWasmStreamingHandler,
  getWasmStreamingHandler,
};
