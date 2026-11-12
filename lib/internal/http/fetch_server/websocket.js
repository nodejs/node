'use strict';

// WebSocket upgrades of the Fetch Server API (Section 11.4, Section 11.5).
// Not yet implemented.

const {
  notSupported,
} = require('internal/http/fetch_server/errors');

function upgradeWebSocket() {
  throw notSupported('WebSocket upgrades are not yet supported');
}

function upgradeWebSocketStream() {
  throw notSupported('WebSocket upgrades are not yet supported');
}

module.exports = {
  upgradeWebSocket,
  upgradeWebSocketStream,
};
