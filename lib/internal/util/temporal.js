'use strict';

// This module is initialized in the pre-execution phase, before user code is
// run. Since the module namespace is not populated at snapshot time, this
// module should be not be imported with a destructuring pattern unless
// imported lazily.

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
    for (const name of constructors.keys()) {
      exports[`isTemporal${name}`] = () => false;
    }
    return;
  }

  for (const { 0: name, 1: brandedGetter } of constructors.entries()) {
    const constructor = Temporal[name];
    exports[`Temporal${name}`] = constructor;

    ArrayPrototypeForEach(
      ObjectEntries(ObjectGetOwnPropertyDescriptors(constructor)),
      ({ 0: key, 1: desc }) => {
        if (typeof key !== 'string' || typeof desc.value !== 'function') return;

        exports[`Temporal${name}${capitalizeKey(key)}`] = desc.value;
      },
    );

    ArrayPrototypeForEach(
      ObjectEntries(ObjectGetOwnPropertyDescriptors(constructor.prototype)),
      ({ 0: key, 1: desc }) => {
        if (typeof key !== 'string') return;

        if ('get' in desc) {
          exports[`Temporal${name}PrototypeGet${capitalizeKey(key)}`] = uncurryThis(desc.get);
          if (key === brandedGetter) {
            exports[`isTemporal${name}`] = makeTypeCheckMethod(desc.get);
          }
        } else {
          exports[`Temporal${name}Prototype${capitalizeKey(key)}`] = typeof desc.value === 'function' ?
            uncurryThis(desc.value) :
            desc.value;
        }
      },
    );
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
