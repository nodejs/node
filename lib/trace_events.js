'use strict';

const {
  ArrayPrototypeJoin,
  SafeSet,
  Symbol,
} = primordials;

const { hasTracing } = internalBinding('config');
const kHandle = Symbol('handle');
const kEnabled = Symbol('enabled');
const kCategories = Symbol('categories');

const kMaxTracingCount = 10;

const {
  ERR_TRACE_EVENTS_CATEGORY_REQUIRED,
  ERR_TRACE_EVENTS_UNAVAILABLE,
} = require('internal/errors').codes;

const { ownsProcessState } = require('internal/worker');
if (!hasTracing || !ownsProcessState)
  throw new ERR_TRACE_EVENTS_UNAVAILABLE();

const { CategorySet, getEnabledCategories } = internalBinding('trace_events');
const { customInspectSymbol } = require('internal/util');
const { format } = require('internal/util/inspect');
const {
  validateObject,
  validateStringArray,
} = require('internal/validators');

const enabledTracingObjects = new SafeSet();

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
          `${kMaxTracingCount} enabled Tracing objects.`,
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
    return ArrayPrototypeJoin(this[kCategories], ',');
  }

  [customInspectSymbol](depth, opts) {
    if (typeof depth === 'number' && depth < 0)
      return this;

    const obj = {
      enabled: this.enabled,
      categories: this.categories,
    };
    return `Tracing ${format(obj)}`;
  }
}

function createTracing(options) {
  validateObject(options, 'options');
  validateStringArray(options.categories, 'options.categories');

  if (options.categories.length <= 0)
    throw new ERR_TRACE_EVENTS_CATEGORY_REQUIRED();

  return new Tracing(options.categories);
}

module.exports = {
  createTracing,
  getEnabledCategories,
};
