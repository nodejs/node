'use strict';

// node::NewContext calls this script

(function(global) {
  // https://github.com/nodejs/node/issues/14909
  if (global.Intl) delete global.Intl.v8BreakIterator;

  // https://github.com/nodejs/node/issues/21219
  // Adds Atomics.notify and warns on first usage of Atomics.wake

  const AtomicsWake = global.Atomics.wake;
  const ReflectApply = global.Reflect.apply;

  // wrap for function.name
  function notify(...args) {
    return ReflectApply(AtomicsWake, this, args);
  }

  const warning = 'Atomics.wake will be removed in a future version, ' +
    'use Atomics.notify instead.';

  let wakeWarned = false;
  function wake(...args) {
    if (!wakeWarned) {
      wakeWarned = true;

      if (global.process !== undefined) {
        global.process.emitWarning(warning, 'Atomics');
      } else {
        global.console.error(`Atomics: ${warning}`);
      }
    }

    return ReflectApply(AtomicsWake, this, args);
  }

  global.Object.defineProperties(global.Atomics, {
    notify: {
      value: notify,
      writable: true,
      enumerable: false,
      configurable: true,
    },
    wake: {
      value: wake,
      writable: true,
      enumerable: false,
      configurable: true,
    },
  });
}(this));
