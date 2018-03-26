'use strict';

const {
  enableTracingCategories: _enableTracingCategories,
  disableTracingCategories: _disableTracingCategories,
  getTracingCategories
} = process.binding('trace_events');

const { trace_events_enabled } = process.binding('config');

const {
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_TRACE_EVENTS_UNAVAILABLE
  }
} = require('internal/errors');

function enableTracingCategories(...categories) {
  if (!trace_events_enabled)
    throw new ERR_TRACE_EVENTS_UNAVAILABLE();

  for (var n = 0; n < categories.length; n++) {
    if (typeof categories[n] !== 'string')
      throw new ERR_INVALID_ARG_TYPE('category', 'string');
  }

  _enableTracingCategories(categories);
}

function disableTracingCategories(...categories) {
  if (!trace_events_enabled)
    throw new ERR_TRACE_EVENTS_UNAVAILABLE();

  for (var n = 0; n < categories.length; n++) {
    if (typeof categories[n] !== 'string')
      throw new ERR_INVALID_ARG_TYPE('category', 'string');
  }

  _disableTracingCategories(categories);
}

module.exports = {
  enableTracingCategories,
  disableTracingCategories,
  getTracingCategories
};
