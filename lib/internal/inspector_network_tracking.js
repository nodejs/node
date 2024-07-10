'use strict';

const {
  DateNow,
} = primordials;

const {
  getOptionValue,
} = require('internal/options');

let dc;
let emitProtocolEvent;

let requestId = 0;
const getNextRequestId = () => `node-network-event-${++requestId}`;

function onClientRequestStart({ request }) {
  const url = `${request.protocol}//${request.host}${request.path}`;
  const wallTime = DateNow();
  const timestamp = wallTime / 1000;
  request._inspectorRequestId = getNextRequestId();
  emitProtocolEvent('NodeNetwork.requestWillBeSent', {
    requestId: request._inspectorRequestId,
    timestamp,
    wallTime,
    request: {
      url,
      method: request.method,
    },
  });
}

function onClientResponseFinish({ request }) {
  if (typeof request._inspectorRequestId !== 'string') {
    return;
  }
  const timestamp = DateNow() / 1000;
  emitProtocolEvent('NodeNetwork.responseReceived', {
    requestId: request._inspectorRequestId,
    timestamp,
  });
  emitProtocolEvent('NodeNetwork.loadingFinished', {
    requestId: request._inspectorRequestId,
    timestamp,
  });
}

function enable() {
  if (!getOptionValue('--experimental-network-inspection')) {
    return;
  }
  if (!dc) {
    dc = require('diagnostics_channel');
  }
  if (!emitProtocolEvent) {
    emitProtocolEvent = require('inspector').emitProtocolEvent;
  }
  dc.subscribe('http.client.request.start', onClientRequestStart);
  dc.subscribe('http.client.response.finish', onClientResponseFinish);
}

function disable() {
  if (!getOptionValue('--experimental-network-inspection')) {
    return;
  }
  dc.unsubscribe('http.client.request.start', onClientRequestStart);
  dc.unsubscribe('http.client.response.finish', onClientResponseFinish);
}

module.exports = {
  enable,
  disable,
};
