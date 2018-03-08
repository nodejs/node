'use strict';

const {
  startTracing,
  stopTracing,
  getTracingCategories
} = process.binding('trace_events');

const { trace_events_enabled } = process.binding('config');

const {
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_TRACE_EVENTS_UNAVAILABLE
  }
} = require('internal/errors');

function setTracingCategories(categories) {
  if (!trace_events_enabled)
    throw new ERR_TRACE_EVENTS_UNAVAILABLE();

  if (categories == null || categories === '')
    return stopTracing();

  if (typeof categories !== 'string')
    throw new ERR_INVALID_ARG_TYPE('categories', 'string');

  startTracing(categories);
}

module.exports = {
  setTracingCategories,
  getTracingCategories
};
