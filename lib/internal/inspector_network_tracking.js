'use strict';

function enable() {
  require('internal/inspector/network_http').enable();
  // TODO: add undici request/websocket tracking.
  // https://github.com/nodejs/node/issues/53946
}

function disable() {
  require('internal/inspector/network_http').disable();
  // TODO: add undici request/websocket tracking.
  // https://github.com/nodejs/node/issues/53946
}

module.exports = {
  enable,
  disable,
};
