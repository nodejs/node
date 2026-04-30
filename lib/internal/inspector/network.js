'use strict';

const {
  ArrayPrototypeForEach,
  NumberMAX_SAFE_INTEGER,
  StringPrototypeToLowerCase,
  Symbol,
} = primordials;

const dc = require('diagnostics_channel');
const { now } = require('internal/perf/utils');
const { MIMEType } = require('internal/mime');

const kInspectorRequestId = Symbol('kInspectorRequestId');

// https://chromedevtools.github.io/devtools-protocol/1-3/Network/#type-ResourceType
const kResourceType = {
  Document: 'Document',
  Stylesheet: 'Stylesheet',
  Image: 'Image',
  Media: 'Media',
  Font: 'Font',
  Script: 'Script',
  TextTrack: 'TextTrack',
  XHR: 'XHR',
  Fetch: 'Fetch',
  Prefetch: 'Prefetch',
  EventSource: 'EventSource',
  WebSocket: 'WebSocket',
  Manifest: 'Manifest',
  SignedExchange: 'SignedExchange',
  Ping: 'Ping',
  CSPViolationReport: 'CSPViolationReport',
  Preflight: 'Preflight',
  Other: 'Other',
};

/**
 * Return a monotonically increasing time in seconds since an arbitrary point in the past.
 * @returns {number}
 */
function getMonotonicTime() {
  return now() / 1000;
}

let requestId = 0;
function getNextRequestId() {
  if (requestId === NumberMAX_SAFE_INTEGER) {
    requestId = 0;
  }
  return `node-network-event-${++requestId}`;
};

function sniffMimeType(contentType) {
  let mimeType;
  let charset;
  try {
    const mimeTypeObj = new MIMEType(contentType);
    mimeType = StringPrototypeToLowerCase(mimeTypeObj.essence || '');
    charset = StringPrototypeToLowerCase(mimeTypeObj.params.get('charset') || '');
  } catch {
    mimeType = '';
    charset = '';
  }

  return {
    __proto__: null,
    mimeType,
    charset,
  };
}

function registerDiagnosticChannels(listenerPairs) {
  function enable() {
    ArrayPrototypeForEach(listenerPairs, ({ 0: channel, 1: listener }) => {
      dc.subscribe(channel, listener);
    });
  }

  function disable() {
    ArrayPrototypeForEach(listenerPairs, ({ 0: channel, 1: listener }) => {
      dc.unsubscribe(channel, listener);
    });
  }

  return {
    enable,
    disable,
  };
}

module.exports = {
  kInspectorRequestId,
  kResourceType,
  getMonotonicTime,
  getNextRequestId,
  registerDiagnosticChannels,
  sniffMimeType,
};
