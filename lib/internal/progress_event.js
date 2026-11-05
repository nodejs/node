'use strict';

// The Web IDL and specification prose quoted below is reproduced verbatim
// from the specification, including its original (lowercase) casing.
// Refs: https://xhr.spec.whatwg.org/#interface-progressevent
/* eslint-disable capitalized-comments */

const {
  ObjectDefineProperties,
  Symbol,
  SymbolToStringTag,
} = primordials;

const {
  Event,
} = require('internal/event_target');

const {
  kEnumerableProperty,
  kEmptyObject,
} = require('internal/util');

const {
  validateThisInternalField,
} = require('internal/validators');

const {
  converters,
  createDictionaryConverter,
  requiredArguments,
} = require('internal/webidl');

const kLengthComputable = Symbol('lengthComputable');
const kLoaded = Symbol('loaded');
const kTotal = Symbol('total');

// dictionary EventInit {
const kEventInit = [
  {
    // boolean bubbles = false;
    key: 'bubbles',
    converter: converters.boolean,
    defaultValue: () => false,
  },
  {
    // boolean cancelable = false;
    key: 'cancelable',
    converter: converters.boolean,
    defaultValue: () => false,
  },
  {
    // boolean composed = false;
    key: 'composed',
    converter: converters.boolean,
    defaultValue: () => false,
  },
];
// };

// dictionary ProgressEventInit : EventInit {
const kProgressEventInit = [
  {
    // boolean lengthComputable = false;
    key: 'lengthComputable',
    converter: converters.boolean,
    defaultValue: () => false,
  },
  {
    // double loaded = 0;
    key: 'loaded',
    converter: converters.double,
    defaultValue: () => 0,
  },
  {
    // double total = 0;
    key: 'total',
    converter: converters.double,
    defaultValue: () => 0,
  },
];
// };

const convertProgressEventInit = createDictionaryConverter(
  'ProgressEventInit',
  [kEventInit, kProgressEventInit],
);

// [Exposed=(Window,Worker)]
// interface ProgressEvent : Event {
class ProgressEvent extends Event {
  // constructor(DOMString type, optional ProgressEventInit eventInitDict = {});
  constructor(type, eventInitDict = kEmptyObject) {
    const prefix = "Failed to construct 'ProgressEvent'";
    requiredArguments(arguments.length, 1, { prefix });
    type = converters.DOMString(type, { prefix, context: 'Argument 1' });
    eventInitDict = convertProgressEventInit(eventInitDict, {
      prefix,
      context: 'Argument 2',
    });
    super(type, eventInitDict);

    // "The lengthComputable, loaded, and total getter steps are to return
    // the value they were initialized to."
    this[kLengthComputable] = eventInitDict.lengthComputable;
    this[kLoaded] = eventInitDict.loaded;
    this[kTotal] = eventInitDict.total;
  }

  // readonly attribute boolean lengthComputable;
  get lengthComputable() {
    validateThisInternalField(this, kLengthComputable, 'ProgressEvent');
    return this[kLengthComputable];
  }

  // readonly attribute double loaded;
  get loaded() {
    validateThisInternalField(this, kLengthComputable, 'ProgressEvent');
    return this[kLoaded];
  }

  // readonly attribute double total;
  get total() {
    validateThisInternalField(this, kLengthComputable, 'ProgressEvent');
    return this[kTotal];
  }
}
// };

ObjectDefineProperties(ProgressEvent.prototype, {
  lengthComputable: kEnumerableProperty,
  loaded: kEnumerableProperty,
  total: kEnumerableProperty,
  [SymbolToStringTag]: {
    __proto__: null,
    configurable: true,
    value: 'ProgressEvent',
  },
});

module.exports = {
  ProgressEvent,
};
