'use strict';

// Node.js does not dispatch the global unhandledrejection event used by
// testharness.js. Ignore rejections only when the harness settings allow them.
module.exports = function honorAllowedRejections() {
  const { setup, promise_setup, add_result_callback, add_completion_callback } = globalThis;
  let configurable = true;
  let allowed = false;
  let singleTest = false;
  const ignoreRejection = () => {};

  // The harness ignores configuration changes once results have started.
  add_result_callback(() => { configurable = false; });
  add_completion_callback(() => { configurable = false; });

  function apply(properties) {
    if (properties) {
      if (Object.prototype.propertyIsEnumerable.call(properties, 'allow_uncaught_exception')) {
        allowed = properties.allow_uncaught_exception;
      }
      if (Object.prototype.propertyIsEnumerable.call(properties, 'single_test') && properties.single_test) {
        singleTest = true;
      }
    }
    process.removeListener('unhandledRejection', ignoreRejection);
    // In single_test mode the harness still fails the implicit test when
    // uncaught errors are allowed. Retain Node's failure path for that mode.
    if (allowed && !singleTest) {
      process.on('unhandledRejection', ignoreRejection);
    }
  }

  globalThis.setup = function(funcOrProperties, maybeProperties) {
    let func;
    let properties;
    if (arguments.length === 2) {
      func = funcOrProperties;
      properties = maybeProperties;
    } else if (funcOrProperties instanceof Function) {
      func = funcOrProperties;
      properties = {};
    } else {
      properties = funcOrProperties;
    }

    // This callback runs only when setup was accepted, before any nested
    // setup calls made by the test's callback can override its settings.
    return setup.call(this, () => {
      apply(properties);
      if (func) func();
    }, properties);
  };

  globalThis.promise_setup = function(func, properties) {
    if (typeof func !== 'function') {
      return promise_setup.call(this, func, properties);
    }
    return promise_setup.call(this, () => {
      // promise_setup applies settings when its queued callback executes.
      // Unlike setup, it still invokes that callback after results start.
      if (configurable) apply(properties);
      return func();
    }, properties);
  };
};
