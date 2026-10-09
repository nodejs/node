'use strict';

// This module is initialized in the pre-execution phase, before user code is
// run. Since the module namespace is not populated at snapshot time, this
// module should be not be imported with a destructuring pattern in a module
// header or used within top-level module code, as the properties may not yet
// be initialized. Either import this module lazily, or import the namespace
// object without destructuring and access its properties at the point of use.

// This module exposes the following exports:
// * `hasTemporal`, a boolean property indicating whether the Temporal builtin
//   is present during initialization.
// * Typecheck methods for Temporal objects: `isTemporalDuration()`,
//   `isTemporalInstant()` etc. If `hasTemporal` is false, then these methods
//   will always return `false`. Note that these are much less performant than
//   the native typecheck methods in internal/util/types, and should not be
//   used in performance-critical paths.
// If `hasTemporal` is true, then this module also exports the following, in
// primordial style:
// * Temporal constructors: `TemporalDuration`, `TemporalInstant` etc.
// * Static methods on temporal constructors: `TemporalDurationCompare`,
//   `TemporalDurationFrom` etc.
// * Temporal prototype properties, methods and getters:
//   `TemporalDurationPrototypeAbs`, `TemporalDurationPrototypeGetBlank` etc.

// TODO(Renegade334): Remove typecheck methods once available via V8 API.
// Replace with primordials if/when temporal_rs becomes a mandatory V8
// build dependency and the harmony_temporal flag is removed.

const {
  ArrayPrototypeForEach,
  FunctionPrototypeCall,
  ObjectEntries,
  ObjectGetOwnPropertyDescriptors,
  SafeMap,
  StringPrototypeSubstring,
  StringPrototypeToUpperCase,
  globalThis,
  uncurryThis,
} = primordials;

// The keys of this Map are the Temporal constructor names.
// The values are the names of the most lightweight brand-aware getter within
// each prototype, which can be called speculatively as a brand check.
const constructors = new SafeMap()
    .set('Duration', 'nanoseconds')
    .set('Instant', 'epochNanoseconds')
    .set('PlainDate', 'calendarId')
    .set('PlainDateTime', 'calendarId')
    .set('PlainMonthDay', 'calendarId')
    .set('PlainTime', 'nanosecond')
    .set('PlainYearMonth', 'calendarId')
    .set('ZonedDateTime', 'calendarId');

exports.initialize = () => {
  const { Temporal } = globalThis;

  if (Temporal === undefined) {
    exports.hasTemporal = false;

    for (const name of constructors.keys()) {
      exports[`isTemporal${name}`] = () => false;
    }

    return;
  }

  exports.hasTemporal = true;

  for (const { 0: name, 1: brandedGetter } of constructors) {
    const constructor = Temporal[name];
    exports[`Temporal${name}`] = constructor;

    ArrayPrototypeForEach(
      ObjectEntries(ObjectGetOwnPropertyDescriptors(constructor)),
      ({ 0: key, 1: desc }) => {
        if (typeof key === 'string' && typeof desc.value === 'function') {
          exports[`Temporal${name}${capitalizeKey(key)}`] = desc.value;
        }
      },
    );

    const descriptors = ObjectGetOwnPropertyDescriptors(constructor.prototype);
    ArrayPrototypeForEach(
      ObjectEntries(descriptors),
      ({ 0: key, 1: desc }) => {
        if (typeof key !== 'string') return;

        if ('get' in desc) {
          exports[`Temporal${name}PrototypeGet${capitalizeKey(key)}`] = uncurryThis(desc.get);
        } else {
          exports[`Temporal${name}Prototype${capitalizeKey(key)}`] = typeof desc.value === 'function' ?
            uncurryThis(desc.value) :
            desc.value;
        }
      },
    );

    exports[`isTemporal${name}`] = makeTypeCheckMethod(descriptors[brandedGetter].get);
  }
};

function makeTypeCheckMethod(getter) {
  return (value) => {
    try {
      FunctionPrototypeCall(getter, value);
      return true;
    } catch {
      return false;
    }
  };
}

function capitalizeKey(key) {
  return `${StringPrototypeToUpperCase(key[0])}${StringPrototypeSubstring(key, 1)}`;
}
