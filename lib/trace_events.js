'use strict';

const { hasTracing } = process.binding('config');
const kHandle = Symbol('handle');
const kEnabled = Symbol('enabled');
const kCategories = Symbol('categories');

const kMaxTracingCount = 10;

const {
  ERR_TRACE_EVENTS_CATEGORY_REQUIRED,
  ERR_TRACE_EVENTS_UNAVAILABLE,
  ERR_INVALID_ARG_TYPE
} = require('internal/errors').codes;

if (!hasTracing)
  throw new ERR_TRACE_EVENTS_UNAVAILABLE();

const { CategorySet, getEnabledCategories } = process.binding('trace_events');
const { customInspectSymbol } = require('internal/util');
const { format } = require('util');

const enabledTracingObjects = new Set();

class Tracing {
  constructor(categories) {
    this[kHandle] = new CategorySet(categories);
    this[kCategories] = categories;
    this[kEnabled] = false;
  }

  enable() {
    if (!this[kEnabled]) {
      this[kEnabled] = true;
      this[kHandle].enable();
      enabledTracingObjects.add(this);
      if (enabledTracingObjects.size > kMaxTracingCount) {
        process.emitWarning(
          'Possible trace_events memory leak detected. There are more than ' +
          `${kMaxTracingCount} enabled Tracing objects.`
        );
      }
    }
  }

  disable() {
    if (this[kEnabled]) {
      this[kEnabled] = false;
      this[kHandle].disable();
      enabledTracingObjects.delete(this);
    }
  }

  get enabled() {
    return this[kEnabled];
  }

  get categories() {
    return this[kCategories].join(',');
  }

  [customInspectSymbol](depth, opts) {
    const obj = {
      enabled: this.enabled,
      categories: this.categories
    };
    return `Tracing ${format(obj)}`;
  }
}

function createTracing(options) {
  if (typeof options !== 'object' || options == null)
    throw new ERR_INVALID_ARG_TYPE('options', 'object', options);

  if (!Array.isArray(options.categories)) {
    throw new ERR_INVALID_ARG_TYPE('options.categories', 'string[]',
                                   options.categories);
  }

  if (options.categories.length <= 0)
    throw new ERR_TRACE_EVENTS_CATEGORY_REQUIRED();

  return new Tracing(options.categories);
}

module.exports = {
  createTracing,
  getEnabledCategories
};
