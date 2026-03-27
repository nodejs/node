'use strict';

const {
  ArrayIsArray,
  SafeSet,
  StringPrototypeSplit,
  StringPrototypeTrim,
} = primordials;

const {
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_ARG_VALUE,
  },
} = require('internal/errors');
const {
  validateInteger,
  validateObject,
  validateString,
} = require('internal/validators');

const { AsyncLocalStorage } = require('async_hooks');
const { URL } = require('internal/url');

const kDefaultEndpoint = 'http://localhost:4318';

let endpoint = null;
let active = false;
let filter = null; // null = all modules enabled; SafeSet = only listed modules.
let spanStorage = null;
let subscriptions = null;
let collectorHost = null; // Normalized host (e.g. "localhost:4318") for HTTP client filtering.

function getSpanStorage() {
  spanStorage ??= new AsyncLocalStorage();
  return spanStorage;
}

function isActive() {
  return active;
}

function getEndpoint() {
  return endpoint;
}

function getCollectorHost() {
  return collectorHost;
}

function isModuleEnabled(moduleName) {
  if (filter == null) return true;
  return filter.has(moduleName);
}

function parseFilter(filter) {
  if (filter == null) return null;
  if (ArrayIsArray(filter)) {
    const set = new SafeSet();
    for (let i = 0; i < filter.length; i++) {
      const trimmed = StringPrototypeTrim(`${filter[i]}`);
      if (trimmed) set.add(trimmed);
    }
    return set;
  }
  if (typeof filter === 'string') {
    const parts = StringPrototypeSplit(filter, ',');
    const set = new SafeSet();
    for (let i = 0; i < parts.length; i++) {
      const trimmed = StringPrototypeTrim(parts[i]);
      if (trimmed) set.add(trimmed);
    }
    return set;
  }
  throw new ERR_INVALID_ARG_TYPE('options.filter',
                                 ['string', 'Array', 'null', 'undefined'],
                                 filter);
}

function start(options = { __proto__: null }) {
  validateObject(options, 'options');

  const endpointValue = options.endpoint ?? kDefaultEndpoint;
  validateString(endpointValue, 'options.endpoint');
  if (!endpointValue) {
    throw new ERR_INVALID_ARG_VALUE('options.endpoint', endpointValue,
                                    'must be a non-empty string');
  }

  let parsed;
  try {
    parsed = new URL(endpointValue);
  } catch {
    throw new ERR_INVALID_ARG_VALUE('options.endpoint', endpointValue,
                                    'must be a valid URL');
  }

  if (options.maxBufferSize !== undefined) {
    validateInteger(options.maxBufferSize, 'options.maxBufferSize', 1);
  }
  if (options.flushInterval !== undefined) {
    validateInteger(options.flushInterval, 'options.flushInterval', 1);
  }

  const parsedFilter = parseFilter(options.filter);

  if (active) {
    stop();
  }

  endpoint = endpointValue;
  filter = parsedFilter;
  active = true;
  collectorHost = parsed.host;

  const { enableInstrumentations } = require('internal/otel/instrumentations');
  subscriptions = enableInstrumentations();

  const { startFlusher } = require('internal/otel/flush');
  startFlusher(options);
}

function stop() {
  if (!active) return;

  const { disableInstrumentations } = require('internal/otel/instrumentations');
  if (subscriptions != null) {
    disableInstrumentations(subscriptions);
    subscriptions = null;
  }

  const { stopFlusher, flush, resetCaches } = require('internal/otel/flush');
  flush();
  stopFlusher();
  resetCaches();

  active = false;
  endpoint = null;
  filter = null;
  collectorHost = null;

  if (spanStorage != null) {
    spanStorage.disable();
    spanStorage = null;
  }
}

module.exports = {
  start,
  stop,
  isActive,
  getEndpoint,
  getSpanStorage,
  isModuleEnabled,
  getCollectorHost,
};
