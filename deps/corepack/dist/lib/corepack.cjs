"use strict";
var __create = Object.create;
var __defProp = Object.defineProperty;
var __getOwnPropDesc = Object.getOwnPropertyDescriptor;
var __getOwnPropNames = Object.getOwnPropertyNames;
var __getProtoOf = Object.getPrototypeOf;
var __hasOwnProp = Object.prototype.hasOwnProperty;
var __esm = (fn2, res) => function __init() {
  return fn2 && (res = (0, fn2[__getOwnPropNames(fn2)[0]])(fn2 = 0)), res;
};
var __commonJS = (cb, mod) => function __require() {
  return mod || (0, cb[__getOwnPropNames(cb)[0]])((mod = { exports: {} }).exports, mod), mod.exports;
};
var __export = (target, all) => {
  for (var name2 in all)
    __defProp(target, name2, { get: all[name2], enumerable: true });
};
var __copyProps = (to, from, except, desc2) => {
  if (from && typeof from === "object" || typeof from === "function") {
    for (let key of __getOwnPropNames(from))
      if (!__hasOwnProp.call(to, key) && key !== except)
        __defProp(to, key, { get: () => from[key], enumerable: !(desc2 = __getOwnPropDesc(from, key)) || desc2.enumerable });
  }
  return to;
};
var __toESM = (mod, isNodeMode, target) => (target = mod != null ? __create(__getProtoOf(mod)) : {}, __copyProps(
  // If the importer is in node compatibility mode or this is not an ESM
  // file that has been converted to a CommonJS file using a Babel-
  // compatible transform (i.e. "__esModule" has not been set), then set
  // "default" to the CommonJS "module.exports" for node compatibility.
  isNodeMode || !mod || !mod.__esModule ? __defProp(target, "default", { value: mod, enumerable: true }) : target,
  mod
));
var __toCommonJS = (mod) => __copyProps(__defProp({}, "__esModule", { value: true }), mod);

// node_modules/clipanion/lib/constants.js
var require_constants = __commonJS({
  "node_modules/clipanion/lib/constants.js"(exports2) {
    "use strict";
    Object.defineProperty(exports2, "__esModule", { value: true });
    var NODE_INITIAL = 0;
    var NODE_SUCCESS = 1;
    var NODE_ERRORED = 2;
    var START_OF_INPUT = ``;
    var END_OF_INPUT = `\0`;
    var HELP_COMMAND_INDEX = -1;
    var HELP_REGEX = /^(-h|--help)(?:=([0-9]+))?$/;
    var OPTION_REGEX = /^(--[a-z]+(?:-[a-z]+)*|-[a-zA-Z]+)$/;
    var BATCH_REGEX = /^-[a-zA-Z]{2,}$/;
    var BINDING_REGEX = /^([^=]+)=([\s\S]*)$/;
    var DEBUG = process.env.DEBUG_CLI === `1`;
    exports2.BATCH_REGEX = BATCH_REGEX;
    exports2.BINDING_REGEX = BINDING_REGEX;
    exports2.DEBUG = DEBUG;
    exports2.END_OF_INPUT = END_OF_INPUT;
    exports2.HELP_COMMAND_INDEX = HELP_COMMAND_INDEX;
    exports2.HELP_REGEX = HELP_REGEX;
    exports2.NODE_ERRORED = NODE_ERRORED;
    exports2.NODE_INITIAL = NODE_INITIAL;
    exports2.NODE_SUCCESS = NODE_SUCCESS;
    exports2.OPTION_REGEX = OPTION_REGEX;
    exports2.START_OF_INPUT = START_OF_INPUT;
  }
});

// node_modules/clipanion/lib/errors.js
var require_errors = __commonJS({
  "node_modules/clipanion/lib/errors.js"(exports2) {
    "use strict";
    Object.defineProperty(exports2, "__esModule", { value: true });
    var constants2 = require_constants();
    var UsageError16 = class extends Error {
      constructor(message) {
        super(message);
        this.clipanion = { type: `usage` };
        this.name = `UsageError`;
      }
    };
    var UnknownSyntaxError = class extends Error {
      constructor(input, candidates) {
        super();
        this.input = input;
        this.candidates = candidates;
        this.clipanion = { type: `none` };
        this.name = `UnknownSyntaxError`;
        if (this.candidates.length === 0) {
          this.message = `Command not found, but we're not sure what's the alternative.`;
        } else if (this.candidates.every((candidate) => candidate.reason !== null && candidate.reason === candidates[0].reason)) {
          const [{ reason }] = this.candidates;
          this.message = `${reason}

${this.candidates.map(({ usage }) => `$ ${usage}`).join(`
`)}`;
        } else if (this.candidates.length === 1) {
          const [{ usage }] = this.candidates;
          this.message = `Command not found; did you mean:

$ ${usage}
${whileRunning(input)}`;
        } else {
          this.message = `Command not found; did you mean one of:

${this.candidates.map(({ usage }, index) => {
            return `${`${index}.`.padStart(4)} ${usage}`;
          }).join(`
`)}

${whileRunning(input)}`;
        }
      }
    };
    var AmbiguousSyntaxError = class extends Error {
      constructor(input, usages) {
        super();
        this.input = input;
        this.usages = usages;
        this.clipanion = { type: `none` };
        this.name = `AmbiguousSyntaxError`;
        this.message = `Cannot find which to pick amongst the following alternatives:

${this.usages.map((usage, index) => {
          return `${`${index}.`.padStart(4)} ${usage}`;
        }).join(`
`)}

${whileRunning(input)}`;
      }
    };
    var whileRunning = (input) => `While running ${input.filter((token) => {
      return token !== constants2.END_OF_INPUT;
    }).map((token) => {
      const json = JSON.stringify(token);
      if (token.match(/\s/) || token.length === 0 || json !== `"${token}"`) {
        return json;
      } else {
        return token;
      }
    }).join(` `)}`;
    exports2.AmbiguousSyntaxError = AmbiguousSyntaxError;
    exports2.UnknownSyntaxError = UnknownSyntaxError;
    exports2.UsageError = UsageError16;
  }
});

// node_modules/clipanion/lib/format.js
var require_format = __commonJS({
  "node_modules/clipanion/lib/format.js"(exports2) {
    "use strict";
    Object.defineProperty(exports2, "__esModule", { value: true });
    var MAX_LINE_LENGTH = 80;
    var richLine = Array(MAX_LINE_LENGTH).fill(`\u2501`);
    for (let t = 0; t <= 24; ++t)
      richLine[richLine.length - t] = `\x1B[38;5;${232 + t}m\u2501`;
    var richFormat = {
      header: (str) => `\x1B[1m\u2501\u2501\u2501 ${str}${str.length < MAX_LINE_LENGTH - 5 ? ` ${richLine.slice(str.length + 5).join(``)}` : `:`}\x1B[0m`,
      bold: (str) => `\x1B[1m${str}\x1B[22m`,
      error: (str) => `\x1B[31m\x1B[1m${str}\x1B[22m\x1B[39m`,
      code: (str) => `\x1B[36m${str}\x1B[39m`
    };
    var textFormat = {
      header: (str) => str,
      bold: (str) => str,
      error: (str) => str,
      code: (str) => str
    };
    function dedent(text) {
      const lines = text.split(`
`);
      const nonEmptyLines = lines.filter((line) => line.match(/\S/));
      const indent = nonEmptyLines.length > 0 ? nonEmptyLines.reduce((minLength, line) => Math.min(minLength, line.length - line.trimStart().length), Number.MAX_VALUE) : 0;
      return lines.map((line) => line.slice(indent).trimRight()).join(`
`);
    }
    function formatMarkdownish(text, { format, paragraphs }) {
      text = text.replace(/\r\n?/g, `
`);
      text = dedent(text);
      text = text.replace(/^\n+|\n+$/g, ``);
      text = text.replace(/^(\s*)-([^\n]*?)\n+/gm, `$1-$2

`);
      text = text.replace(/\n(\n)?\n*/g, ($0, $1) => $1 ? $1 : ` `);
      if (paragraphs) {
        text = text.split(/\n/).map((paragraph) => {
          const bulletMatch = paragraph.match(/^\s*[*-][\t ]+(.*)/);
          if (!bulletMatch)
            return paragraph.match(/(.{1,80})(?: |$)/g).join(`
`);
          const indent = paragraph.length - paragraph.trimStart().length;
          return bulletMatch[1].match(new RegExp(`(.{1,${78 - indent}})(?: |$)`, `g`)).map((line, index) => {
            return ` `.repeat(indent) + (index === 0 ? `- ` : `  `) + line;
          }).join(`
`);
        }).join(`

`);
      }
      text = text.replace(/(`+)((?:.|[\n])*?)\1/g, ($0, $1, $2) => {
        return format.code($1 + $2 + $1);
      });
      text = text.replace(/(\*\*)((?:.|[\n])*?)\1/g, ($0, $1, $2) => {
        return format.bold($1 + $2 + $1);
      });
      return text ? `${text}
` : ``;
    }
    exports2.formatMarkdownish = formatMarkdownish;
    exports2.richFormat = richFormat;
    exports2.textFormat = textFormat;
  }
});

// node_modules/clipanion/lib/advanced/options/utils.js
var require_utils = __commonJS({
  "node_modules/clipanion/lib/advanced/options/utils.js"(exports2) {
    "use strict";
    Object.defineProperty(exports2, "__esModule", { value: true });
    var errors = require_errors();
    var isOptionSymbol = /* @__PURE__ */ Symbol(`clipanion/isOption`);
    function makeCommandOption(spec) {
      return { ...spec, [isOptionSymbol]: true };
    }
    function rerouteArguments(a, b) {
      if (typeof a === `undefined`)
        return [a, b];
      if (typeof a === `object` && a !== null && !Array.isArray(a)) {
        return [void 0, a];
      } else {
        return [a, b];
      }
    }
    function cleanValidationError(message, { mergeName = false } = {}) {
      const match = message.match(/^([^:]+): (.*)$/m);
      if (!match)
        return `validation failed`;
      let [, path16, line] = match;
      if (mergeName)
        line = line[0].toLowerCase() + line.slice(1);
      line = path16 !== `.` || !mergeName ? `${path16.replace(/^\.(\[|$)/, `$1`)}: ${line}` : `: ${line}`;
      return line;
    }
    function formatError(message, errors$1) {
      if (errors$1.length === 1) {
        return new errors.UsageError(`${message}${cleanValidationError(errors$1[0], { mergeName: true })}`);
      } else {
        return new errors.UsageError(`${message}:
${errors$1.map((error) => `
- ${cleanValidationError(error)}`).join(``)}`);
      }
    }
    function applyValidator(name2, value, validator) {
      if (typeof validator === `undefined`)
        return value;
      const errors2 = [];
      const coercions = [];
      const coercion = (v) => {
        const orig = value;
        value = v;
        return coercion.bind(null, orig);
      };
      const check = validator(value, { errors: errors2, coercions, coercion });
      if (!check)
        throw formatError(`Invalid value for ${name2}`, errors2);
      for (const [, op] of coercions)
        op();
      return value;
    }
    exports2.applyValidator = applyValidator;
    exports2.cleanValidationError = cleanValidationError;
    exports2.formatError = formatError;
    exports2.isOptionSymbol = isOptionSymbol;
    exports2.makeCommandOption = makeCommandOption;
    exports2.rerouteArguments = rerouteArguments;
  }
});

// node_modules/typanion/lib/index.mjs
var lib_exports = {};
__export(lib_exports, {
  KeyRelationship: () => KeyRelationship,
  TypeAssertionError: () => TypeAssertionError,
  applyCascade: () => applyCascade,
  as: () => as,
  assert: () => assert,
  assertWithErrors: () => assertWithErrors,
  cascade: () => cascade,
  fn: () => fn,
  hasAtLeastOneKey: () => hasAtLeastOneKey,
  hasExactLength: () => hasExactLength,
  hasForbiddenKeys: () => hasForbiddenKeys,
  hasKeyRelationship: () => hasKeyRelationship,
  hasMaxLength: () => hasMaxLength,
  hasMinLength: () => hasMinLength,
  hasMutuallyExclusiveKeys: () => hasMutuallyExclusiveKeys,
  hasRequiredKeys: () => hasRequiredKeys,
  hasUniqueItems: () => hasUniqueItems,
  isArray: () => isArray,
  isAtLeast: () => isAtLeast,
  isAtMost: () => isAtMost,
  isBase64: () => isBase64,
  isBoolean: () => isBoolean,
  isDate: () => isDate,
  isDict: () => isDict,
  isEnum: () => isEnum,
  isHexColor: () => isHexColor,
  isISO8601: () => isISO8601,
  isInExclusiveRange: () => isInExclusiveRange,
  isInInclusiveRange: () => isInInclusiveRange,
  isInstanceOf: () => isInstanceOf,
  isInteger: () => isInteger,
  isJSON: () => isJSON,
  isLiteral: () => isLiteral,
  isLowerCase: () => isLowerCase,
  isMap: () => isMap,
  isNegative: () => isNegative,
  isNullable: () => isNullable,
  isNumber: () => isNumber,
  isObject: () => isObject,
  isOneOf: () => isOneOf,
  isOptional: () => isOptional,
  isPartial: () => isPartial,
  isPayload: () => isPayload,
  isPositive: () => isPositive,
  isRecord: () => isRecord,
  isSet: () => isSet,
  isString: () => isString,
  isTuple: () => isTuple,
  isUUID4: () => isUUID4,
  isUnknown: () => isUnknown,
  isUpperCase: () => isUpperCase,
  makeTrait: () => makeTrait,
  makeValidator: () => makeValidator,
  matchesRegExp: () => matchesRegExp,
  softAssert: () => softAssert
});
function getPrintable(value) {
  if (value === null)
    return `null`;
  if (value === void 0)
    return `undefined`;
  if (value === ``)
    return `an empty string`;
  if (typeof value === "symbol")
    return `<${value.toString()}>`;
  if (Array.isArray(value))
    return `an array`;
  return JSON.stringify(value);
}
function getPrintableArray(value, conjunction) {
  if (value.length === 0)
    return `nothing`;
  if (value.length === 1)
    return getPrintable(value[0]);
  const rest = value.slice(0, -1);
  const trailing = value[value.length - 1];
  const separator = value.length > 2 ? `, ${conjunction} ` : ` ${conjunction} `;
  return `${rest.map((value2) => getPrintable(value2)).join(`, `)}${separator}${getPrintable(trailing)}`;
}
function computeKey(state, key) {
  var _a, _b, _c;
  if (typeof key === `number`) {
    return `${(_a = state === null || state === void 0 ? void 0 : state.p) !== null && _a !== void 0 ? _a : `.`}[${key}]`;
  } else if (simpleKeyRegExp.test(key)) {
    return `${(_b = state === null || state === void 0 ? void 0 : state.p) !== null && _b !== void 0 ? _b : ``}.${key}`;
  } else {
    return `${(_c = state === null || state === void 0 ? void 0 : state.p) !== null && _c !== void 0 ? _c : `.`}[${JSON.stringify(key)}]`;
  }
}
function plural(n, singular, plural2) {
  return n === 1 ? singular : plural2;
}
function pushError({ errors, p } = {}, message) {
  errors === null || errors === void 0 ? void 0 : errors.push(`${p !== null && p !== void 0 ? p : `.`}: ${message}`);
  return false;
}
function makeSetter(target, key) {
  return (v) => {
    target[key] = v;
  };
}
function makeCoercionFn(target, key) {
  return (v) => {
    const previous = target[key];
    target[key] = v;
    return makeCoercionFn(target, key).bind(null, previous);
  };
}
function makeLazyCoercionFn(fn2, orig, generator) {
  const commit = () => {
    fn2(generator());
    return revert;
  };
  const revert = () => {
    fn2(orig);
    return commit;
  };
  return commit;
}
function isUnknown() {
  return makeValidator({
    test: (value, state) => {
      return true;
    }
  });
}
function isLiteral(expected) {
  return makeValidator({
    test: (value, state) => {
      if (value !== expected)
        return pushError(state, `Expected ${getPrintable(expected)} (got ${getPrintable(value)})`);
      return true;
    }
  });
}
function isString() {
  return makeValidator({
    test: (value, state) => {
      if (typeof value !== `string`)
        return pushError(state, `Expected a string (got ${getPrintable(value)})`);
      return true;
    }
  });
}
function isEnum(enumSpec) {
  const valuesArray = Array.isArray(enumSpec) ? enumSpec : Object.values(enumSpec);
  const isAlphaNum = valuesArray.every((item) => typeof item === "string" || typeof item === "number");
  const values = new Set(valuesArray);
  if (values.size === 1)
    return isLiteral([...values][0]);
  return makeValidator({
    test: (value, state) => {
      if (!values.has(value)) {
        if (isAlphaNum) {
          return pushError(state, `Expected one of ${getPrintableArray(valuesArray, `or`)} (got ${getPrintable(value)})`);
        } else {
          return pushError(state, `Expected a valid enumeration value (got ${getPrintable(value)})`);
        }
      }
      return true;
    }
  });
}
function isBoolean() {
  return makeValidator({
    test: (value, state) => {
      var _a;
      if (typeof value !== `boolean`) {
        if (typeof (state === null || state === void 0 ? void 0 : state.coercions) !== `undefined`) {
          if (typeof (state === null || state === void 0 ? void 0 : state.coercion) === `undefined`)
            return pushError(state, `Unbound coercion result`);
          const coercion = BOOLEAN_COERCIONS.get(value);
          if (typeof coercion !== `undefined`) {
            state.coercions.push([(_a = state.p) !== null && _a !== void 0 ? _a : `.`, state.coercion.bind(null, coercion)]);
            return true;
          }
        }
        return pushError(state, `Expected a boolean (got ${getPrintable(value)})`);
      }
      return true;
    }
  });
}
function isNumber() {
  return makeValidator({
    test: (value, state) => {
      var _a;
      if (typeof value !== `number`) {
        if (typeof (state === null || state === void 0 ? void 0 : state.coercions) !== `undefined`) {
          if (typeof (state === null || state === void 0 ? void 0 : state.coercion) === `undefined`)
            return pushError(state, `Unbound coercion result`);
          let coercion;
          if (typeof value === `string`) {
            let val;
            try {
              val = JSON.parse(value);
            } catch (_b) {
            }
            if (typeof val === `number`) {
              if (JSON.stringify(val) === value) {
                coercion = val;
              } else {
                return pushError(state, `Received a number that can't be safely represented by the runtime (${value})`);
              }
            }
          }
          if (typeof coercion !== `undefined`) {
            state.coercions.push([(_a = state.p) !== null && _a !== void 0 ? _a : `.`, state.coercion.bind(null, coercion)]);
            return true;
          }
        }
        return pushError(state, `Expected a number (got ${getPrintable(value)})`);
      }
      return true;
    }
  });
}
function isPayload(spec) {
  return makeValidator({
    test: (value, state) => {
      var _a;
      if (typeof (state === null || state === void 0 ? void 0 : state.coercions) === `undefined`)
        return pushError(state, `The isPayload predicate can only be used with coercion enabled`);
      if (typeof state.coercion === `undefined`)
        return pushError(state, `Unbound coercion result`);
      if (typeof value !== `string`)
        return pushError(state, `Expected a string (got ${getPrintable(value)})`);
      let inner;
      try {
        inner = JSON.parse(value);
      } catch (_b) {
        return pushError(state, `Expected a JSON string (got ${getPrintable(value)})`);
      }
      const wrapper = { value: inner };
      if (!spec(inner, Object.assign(Object.assign({}, state), { coercion: makeCoercionFn(wrapper, `value`) })))
        return false;
      state.coercions.push([(_a = state.p) !== null && _a !== void 0 ? _a : `.`, state.coercion.bind(null, wrapper.value)]);
      return true;
    }
  });
}
function isDate() {
  return makeValidator({
    test: (value, state) => {
      var _a;
      if (!(value instanceof Date)) {
        if (typeof (state === null || state === void 0 ? void 0 : state.coercions) !== `undefined`) {
          if (typeof (state === null || state === void 0 ? void 0 : state.coercion) === `undefined`)
            return pushError(state, `Unbound coercion result`);
          let coercion;
          if (typeof value === `string` && iso8601RegExp.test(value)) {
            coercion = new Date(value);
          } else {
            let timestamp;
            if (typeof value === `string`) {
              let val;
              try {
                val = JSON.parse(value);
              } catch (_b) {
              }
              if (typeof val === `number`) {
                timestamp = val;
              }
            } else if (typeof value === `number`) {
              timestamp = value;
            }
            if (typeof timestamp !== `undefined`) {
              if (Number.isSafeInteger(timestamp) || !Number.isSafeInteger(timestamp * 1e3)) {
                coercion = new Date(timestamp * 1e3);
              } else {
                return pushError(state, `Received a timestamp that can't be safely represented by the runtime (${value})`);
              }
            }
          }
          if (typeof coercion !== `undefined`) {
            state.coercions.push([(_a = state.p) !== null && _a !== void 0 ? _a : `.`, state.coercion.bind(null, coercion)]);
            return true;
          }
        }
        return pushError(state, `Expected a date (got ${getPrintable(value)})`);
      }
      return true;
    }
  });
}
function isArray(spec, { delimiter } = {}) {
  return makeValidator({
    test: (value, state) => {
      var _a;
      const originalValue = value;
      if (typeof value === `string` && typeof delimiter !== `undefined`) {
        if (typeof (state === null || state === void 0 ? void 0 : state.coercions) !== `undefined`) {
          if (typeof (state === null || state === void 0 ? void 0 : state.coercion) === `undefined`)
            return pushError(state, `Unbound coercion result`);
          value = value.split(delimiter);
        }
      }
      if (!Array.isArray(value))
        return pushError(state, `Expected an array (got ${getPrintable(value)})`);
      let valid = true;
      for (let t = 0, T = value.length; t < T; ++t) {
        valid = spec(value[t], Object.assign(Object.assign({}, state), { p: computeKey(state, t), coercion: makeCoercionFn(value, t) })) && valid;
        if (!valid && (state === null || state === void 0 ? void 0 : state.errors) == null) {
          break;
        }
      }
      if (value !== originalValue)
        state.coercions.push([(_a = state.p) !== null && _a !== void 0 ? _a : `.`, state.coercion.bind(null, value)]);
      return valid;
    }
  });
}
function isSet(spec, { delimiter } = {}) {
  const isArrayValidator = isArray(spec, { delimiter });
  return makeValidator({
    test: (value, state) => {
      var _a, _b;
      if (Object.getPrototypeOf(value).toString() === `[object Set]`) {
        if (typeof (state === null || state === void 0 ? void 0 : state.coercions) !== `undefined`) {
          if (typeof (state === null || state === void 0 ? void 0 : state.coercion) === `undefined`)
            return pushError(state, `Unbound coercion result`);
          const originalValues = [...value];
          const coercedValues = [...value];
          if (!isArrayValidator(coercedValues, Object.assign(Object.assign({}, state), { coercion: void 0 })))
            return false;
          const updateValue = () => coercedValues.some((val, t) => val !== originalValues[t]) ? new Set(coercedValues) : value;
          state.coercions.push([(_a = state.p) !== null && _a !== void 0 ? _a : `.`, makeLazyCoercionFn(state.coercion, value, updateValue)]);
          return true;
        } else {
          let valid = true;
          for (const subValue of value) {
            valid = spec(subValue, Object.assign({}, state)) && valid;
            if (!valid && (state === null || state === void 0 ? void 0 : state.errors) == null) {
              break;
            }
          }
          return valid;
        }
      }
      if (typeof (state === null || state === void 0 ? void 0 : state.coercions) !== `undefined`) {
        if (typeof (state === null || state === void 0 ? void 0 : state.coercion) === `undefined`)
          return pushError(state, `Unbound coercion result`);
        const store = { value };
        if (!isArrayValidator(value, Object.assign(Object.assign({}, state), { coercion: makeCoercionFn(store, `value`) })))
          return false;
        state.coercions.push([(_b = state.p) !== null && _b !== void 0 ? _b : `.`, makeLazyCoercionFn(state.coercion, value, () => new Set(store.value))]);
        return true;
      }
      return pushError(state, `Expected a set (got ${getPrintable(value)})`);
    }
  });
}
function isMap(keySpec, valueSpec) {
  const isArrayValidator = isArray(isTuple([keySpec, valueSpec]));
  const isRecordValidator = isRecord(valueSpec, { keys: keySpec });
  return makeValidator({
    test: (value, state) => {
      var _a, _b, _c;
      if (Object.getPrototypeOf(value).toString() === `[object Map]`) {
        if (typeof (state === null || state === void 0 ? void 0 : state.coercions) !== `undefined`) {
          if (typeof (state === null || state === void 0 ? void 0 : state.coercion) === `undefined`)
            return pushError(state, `Unbound coercion result`);
          const originalValues = [...value];
          const coercedValues = [...value];
          if (!isArrayValidator(coercedValues, Object.assign(Object.assign({}, state), { coercion: void 0 })))
            return false;
          const updateValue = () => coercedValues.some((val, t) => val[0] !== originalValues[t][0] || val[1] !== originalValues[t][1]) ? new Map(coercedValues) : value;
          state.coercions.push([(_a = state.p) !== null && _a !== void 0 ? _a : `.`, makeLazyCoercionFn(state.coercion, value, updateValue)]);
          return true;
        } else {
          let valid = true;
          for (const [key, subValue] of value) {
            valid = keySpec(key, Object.assign({}, state)) && valid;
            if (!valid && (state === null || state === void 0 ? void 0 : state.errors) == null) {
              break;
            }
            valid = valueSpec(subValue, Object.assign(Object.assign({}, state), { p: computeKey(state, key) })) && valid;
            if (!valid && (state === null || state === void 0 ? void 0 : state.errors) == null) {
              break;
            }
          }
          return valid;
        }
      }
      if (typeof (state === null || state === void 0 ? void 0 : state.coercions) !== `undefined`) {
        if (typeof (state === null || state === void 0 ? void 0 : state.coercion) === `undefined`)
          return pushError(state, `Unbound coercion result`);
        const store = { value };
        if (Array.isArray(value)) {
          if (!isArrayValidator(value, Object.assign(Object.assign({}, state), { coercion: void 0 })))
            return false;
          state.coercions.push([(_b = state.p) !== null && _b !== void 0 ? _b : `.`, makeLazyCoercionFn(state.coercion, value, () => new Map(store.value))]);
          return true;
        } else {
          if (!isRecordValidator(value, Object.assign(Object.assign({}, state), { coercion: makeCoercionFn(store, `value`) })))
            return false;
          state.coercions.push([(_c = state.p) !== null && _c !== void 0 ? _c : `.`, makeLazyCoercionFn(state.coercion, value, () => new Map(Object.entries(store.value)))]);
          return true;
        }
      }
      return pushError(state, `Expected a map (got ${getPrintable(value)})`);
    }
  });
}
function isTuple(spec, { delimiter } = {}) {
  const lengthValidator = hasExactLength(spec.length);
  return makeValidator({
    test: (value, state) => {
      var _a;
      if (typeof value === `string` && typeof delimiter !== `undefined`) {
        if (typeof (state === null || state === void 0 ? void 0 : state.coercions) !== `undefined`) {
          if (typeof (state === null || state === void 0 ? void 0 : state.coercion) === `undefined`)
            return pushError(state, `Unbound coercion result`);
          value = value.split(delimiter);
          state.coercions.push([(_a = state.p) !== null && _a !== void 0 ? _a : `.`, state.coercion.bind(null, value)]);
        }
      }
      if (!Array.isArray(value))
        return pushError(state, `Expected a tuple (got ${getPrintable(value)})`);
      let valid = lengthValidator(value, Object.assign({}, state));
      for (let t = 0, T = value.length; t < T && t < spec.length; ++t) {
        valid = spec[t](value[t], Object.assign(Object.assign({}, state), { p: computeKey(state, t), coercion: makeCoercionFn(value, t) })) && valid;
        if (!valid && (state === null || state === void 0 ? void 0 : state.errors) == null) {
          break;
        }
      }
      return valid;
    }
  });
}
function isRecord(spec, { keys: keySpec = null } = {}) {
  const isArrayValidator = isArray(isTuple([keySpec !== null && keySpec !== void 0 ? keySpec : isString(), spec]));
  return makeValidator({
    test: (value, state) => {
      var _a;
      if (Array.isArray(value)) {
        if (typeof (state === null || state === void 0 ? void 0 : state.coercions) !== `undefined`) {
          if (typeof (state === null || state === void 0 ? void 0 : state.coercion) === `undefined`)
            return pushError(state, `Unbound coercion result`);
          if (!isArrayValidator(value, Object.assign(Object.assign({}, state), { coercion: void 0 })))
            return false;
          value = Object.fromEntries(value);
          state.coercions.push([(_a = state.p) !== null && _a !== void 0 ? _a : `.`, state.coercion.bind(null, value)]);
          return true;
        }
      }
      if (typeof value !== `object` || value === null)
        return pushError(state, `Expected an object (got ${getPrintable(value)})`);
      const keys = Object.keys(value);
      let valid = true;
      for (let t = 0, T = keys.length; t < T && (valid || (state === null || state === void 0 ? void 0 : state.errors) != null); ++t) {
        const key = keys[t];
        const sub = value[key];
        if (key === `__proto__` || key === `constructor`) {
          valid = pushError(Object.assign(Object.assign({}, state), { p: computeKey(state, key) }), `Unsafe property name`);
          continue;
        }
        if (keySpec !== null && !keySpec(key, state)) {
          valid = false;
          continue;
        }
        if (!spec(sub, Object.assign(Object.assign({}, state), { p: computeKey(state, key), coercion: makeCoercionFn(value, key) }))) {
          valid = false;
          continue;
        }
      }
      return valid;
    }
  });
}
function isDict(spec, opts = {}) {
  return isRecord(spec, opts);
}
function isObject(props, { extra: extraSpec = null } = {}) {
  const specKeys = Object.keys(props);
  const validator = makeValidator({
    test: (value, state) => {
      if (typeof value !== `object` || value === null)
        return pushError(state, `Expected an object (got ${getPrintable(value)})`);
      const keys = /* @__PURE__ */ new Set([...specKeys, ...Object.keys(value)]);
      const extra = {};
      let valid = true;
      for (const key of keys) {
        if (key === `constructor` || key === `__proto__`) {
          valid = pushError(Object.assign(Object.assign({}, state), { p: computeKey(state, key) }), `Unsafe property name`);
        } else {
          const spec = Object.prototype.hasOwnProperty.call(props, key) ? props[key] : void 0;
          const sub = Object.prototype.hasOwnProperty.call(value, key) ? value[key] : void 0;
          if (typeof spec !== `undefined`) {
            valid = spec(sub, Object.assign(Object.assign({}, state), { p: computeKey(state, key), coercion: makeCoercionFn(value, key) })) && valid;
          } else if (extraSpec === null) {
            valid = pushError(Object.assign(Object.assign({}, state), { p: computeKey(state, key) }), `Extraneous property (got ${getPrintable(sub)})`);
          } else {
            Object.defineProperty(extra, key, {
              enumerable: true,
              get: () => sub,
              set: makeSetter(value, key)
            });
          }
        }
        if (!valid && (state === null || state === void 0 ? void 0 : state.errors) == null) {
          break;
        }
      }
      if (extraSpec !== null && (valid || (state === null || state === void 0 ? void 0 : state.errors) != null))
        valid = extraSpec(extra, state) && valid;
      return valid;
    }
  });
  return Object.assign(validator, {
    properties: props
  });
}
function isPartial(props) {
  return isObject(props, { extra: isRecord(isUnknown()) });
}
function makeTrait(value) {
  return () => {
    return value;
  };
}
function makeValidator({ test }) {
  return makeTrait(test)();
}
function assert(val, validator) {
  if (!validator(val)) {
    throw new TypeAssertionError();
  }
}
function assertWithErrors(val, validator) {
  const errors = [];
  if (!validator(val, { errors })) {
    throw new TypeAssertionError({ errors });
  }
}
function softAssert(val, validator) {
}
function as(value, validator, { coerce = false, errors: storeErrors, throw: throws } = {}) {
  const errors = storeErrors ? [] : void 0;
  if (!coerce) {
    if (validator(value, { errors })) {
      return throws ? value : { value, errors: void 0 };
    } else if (!throws) {
      return { value: void 0, errors: errors !== null && errors !== void 0 ? errors : true };
    } else {
      throw new TypeAssertionError({ errors });
    }
  }
  const state = { value };
  const coercion = makeCoercionFn(state, `value`);
  const coercions = [];
  if (!validator(value, { errors, coercion, coercions })) {
    if (!throws) {
      return { value: void 0, errors: errors !== null && errors !== void 0 ? errors : true };
    } else {
      throw new TypeAssertionError({ errors });
    }
  }
  for (const [, apply] of coercions)
    apply();
  if (throws) {
    return state.value;
  } else {
    return { value: state.value, errors: void 0 };
  }
}
function fn(validators, fn2) {
  const isValidArgList = isTuple(validators);
  return ((...args) => {
    const check = isValidArgList(args);
    if (!check)
      throw new TypeAssertionError();
    return fn2(...args);
  });
}
function hasMinLength(length) {
  return makeValidator({
    test: (value, state) => {
      if (!(value.length >= length))
        return pushError(state, `Expected to have a length of at least ${length} elements (got ${value.length})`);
      return true;
    }
  });
}
function hasMaxLength(length) {
  return makeValidator({
    test: (value, state) => {
      if (!(value.length <= length))
        return pushError(state, `Expected to have a length of at most ${length} elements (got ${value.length})`);
      return true;
    }
  });
}
function hasExactLength(length) {
  return makeValidator({
    test: (value, state) => {
      if (!(value.length === length))
        return pushError(state, `Expected to have a length of exactly ${length} elements (got ${value.length})`);
      return true;
    }
  });
}
function hasUniqueItems({ map } = {}) {
  return makeValidator({
    test: (value, state) => {
      const set = /* @__PURE__ */ new Set();
      const dup = /* @__PURE__ */ new Set();
      for (let t = 0, T = value.length; t < T; ++t) {
        const sub = value[t];
        const key = typeof map !== `undefined` ? map(sub) : sub;
        if (set.has(key)) {
          if (dup.has(key))
            continue;
          pushError(state, `Expected to contain unique elements; got a duplicate with ${getPrintable(value)}`);
          dup.add(key);
        } else {
          set.add(key);
        }
      }
      return dup.size === 0;
    }
  });
}
function isNegative() {
  return makeValidator({
    test: (value, state) => {
      if (!(value <= 0))
        return pushError(state, `Expected to be negative (got ${value})`);
      return true;
    }
  });
}
function isPositive() {
  return makeValidator({
    test: (value, state) => {
      if (!(value >= 0))
        return pushError(state, `Expected to be positive (got ${value})`);
      return true;
    }
  });
}
function isAtLeast(n) {
  return makeValidator({
    test: (value, state) => {
      if (!(value >= n))
        return pushError(state, `Expected to be at least ${n} (got ${value})`);
      return true;
    }
  });
}
function isAtMost(n) {
  return makeValidator({
    test: (value, state) => {
      if (!(value <= n))
        return pushError(state, `Expected to be at most ${n} (got ${value})`);
      return true;
    }
  });
}
function isInInclusiveRange(a, b) {
  return makeValidator({
    test: (value, state) => {
      if (!(value >= a && value <= b))
        return pushError(state, `Expected to be in the [${a}; ${b}] range (got ${value})`);
      return true;
    }
  });
}
function isInExclusiveRange(a, b) {
  return makeValidator({
    test: (value, state) => {
      if (!(value >= a && value < b))
        return pushError(state, `Expected to be in the [${a}; ${b}[ range (got ${value})`);
      return true;
    }
  });
}
function isInteger({ unsafe = false } = {}) {
  return makeValidator({
    test: (value, state) => {
      if (value !== Math.round(value))
        return pushError(state, `Expected to be an integer (got ${value})`);
      if (!unsafe && !Number.isSafeInteger(value))
        return pushError(state, `Expected to be a safe integer (got ${value})`);
      return true;
    }
  });
}
function matchesRegExp(regExp) {
  return makeValidator({
    test: (value, state) => {
      if (!regExp.test(value))
        return pushError(state, `Expected to match the pattern ${regExp.toString()} (got ${getPrintable(value)})`);
      return true;
    }
  });
}
function isLowerCase() {
  return makeValidator({
    test: (value, state) => {
      if (value !== value.toLowerCase())
        return pushError(state, `Expected to be all-lowercase (got ${value})`);
      return true;
    }
  });
}
function isUpperCase() {
  return makeValidator({
    test: (value, state) => {
      if (value !== value.toUpperCase())
        return pushError(state, `Expected to be all-uppercase (got ${value})`);
      return true;
    }
  });
}
function isUUID4() {
  return makeValidator({
    test: (value, state) => {
      if (!uuid4RegExp.test(value))
        return pushError(state, `Expected to be a valid UUID v4 (got ${getPrintable(value)})`);
      return true;
    }
  });
}
function isISO8601() {
  return makeValidator({
    test: (value, state) => {
      if (!iso8601RegExp.test(value))
        return pushError(state, `Expected to be a valid ISO 8601 date string (got ${getPrintable(value)})`);
      return true;
    }
  });
}
function isHexColor({ alpha = false }) {
  return makeValidator({
    test: (value, state) => {
      const res = alpha ? colorStringRegExp.test(value) : colorStringAlphaRegExp.test(value);
      if (!res)
        return pushError(state, `Expected to be a valid hexadecimal color string (got ${getPrintable(value)})`);
      return true;
    }
  });
}
function isBase64() {
  return makeValidator({
    test: (value, state) => {
      if (!base64RegExp.test(value))
        return pushError(state, `Expected to be a valid base 64 string (got ${getPrintable(value)})`);
      return true;
    }
  });
}
function isJSON(spec = isUnknown()) {
  return makeValidator({
    test: (value, state) => {
      let data;
      try {
        data = JSON.parse(value);
      } catch (_a) {
        return pushError(state, `Expected to be a valid JSON string (got ${getPrintable(value)})`);
      }
      return spec(data, state);
    }
  });
}
function cascade(spec, ...followups) {
  const resolvedFollowups = Array.isArray(followups[0]) ? followups[0] : followups;
  return makeValidator({
    test: (value, state) => {
      var _a, _b;
      const context = { value };
      const subCoercion = typeof (state === null || state === void 0 ? void 0 : state.coercions) !== `undefined` ? makeCoercionFn(context, `value`) : void 0;
      const subCoercions = typeof (state === null || state === void 0 ? void 0 : state.coercions) !== `undefined` ? [] : void 0;
      if (!spec(value, Object.assign(Object.assign({}, state), { coercion: subCoercion, coercions: subCoercions })))
        return false;
      const reverts = [];
      if (typeof subCoercions !== `undefined`)
        for (const [, coercion] of subCoercions)
          reverts.push(coercion());
      try {
        if (typeof (state === null || state === void 0 ? void 0 : state.coercions) !== `undefined`) {
          if (context.value !== value) {
            if (typeof (state === null || state === void 0 ? void 0 : state.coercion) === `undefined`)
              return pushError(state, `Unbound coercion result`);
            state.coercions.push([(_a = state.p) !== null && _a !== void 0 ? _a : `.`, state.coercion.bind(null, context.value)]);
          }
          (_b = state === null || state === void 0 ? void 0 : state.coercions) === null || _b === void 0 ? void 0 : _b.push(...subCoercions);
        }
        return resolvedFollowups.every((spec2) => {
          return spec2(context.value, state);
        });
      } finally {
        for (const revert of reverts) {
          revert();
        }
      }
    }
  });
}
function applyCascade(spec, ...followups) {
  const resolvedFollowups = Array.isArray(followups[0]) ? followups[0] : followups;
  return cascade(spec, resolvedFollowups);
}
function isOptional(spec) {
  return makeValidator({
    test: (value, state) => {
      if (typeof value === `undefined`)
        return true;
      return spec(value, state);
    }
  });
}
function isNullable(spec) {
  return makeValidator({
    test: (value, state) => {
      if (value === null)
        return true;
      return spec(value, state);
    }
  });
}
function hasRequiredKeys(requiredKeys, options) {
  var _a;
  const requiredSet = new Set(requiredKeys);
  const check = checks[(_a = options === null || options === void 0 ? void 0 : options.missingIf) !== null && _a !== void 0 ? _a : "missing"];
  return makeValidator({
    test: (value, state) => {
      const keys = new Set(Object.keys(value));
      const problems = [];
      for (const key of requiredSet)
        if (!check(keys, key, value))
          problems.push(key);
      if (problems.length > 0)
        return pushError(state, `Missing required ${plural(problems.length, `property`, `properties`)} ${getPrintableArray(problems, `and`)}`);
      return true;
    }
  });
}
function hasAtLeastOneKey(requiredKeys, options) {
  var _a;
  const requiredSet = new Set(requiredKeys);
  const check = checks[(_a = options === null || options === void 0 ? void 0 : options.missingIf) !== null && _a !== void 0 ? _a : "missing"];
  return makeValidator({
    test: (value, state) => {
      const keys = Object.keys(value);
      const valid = keys.some((key) => check(requiredSet, key, value));
      if (!valid)
        return pushError(state, `Missing at least one property from ${getPrintableArray(Array.from(requiredSet), `or`)}`);
      return true;
    }
  });
}
function hasForbiddenKeys(forbiddenKeys, options) {
  var _a;
  const forbiddenSet = new Set(forbiddenKeys);
  const check = checks[(_a = options === null || options === void 0 ? void 0 : options.missingIf) !== null && _a !== void 0 ? _a : "missing"];
  return makeValidator({
    test: (value, state) => {
      const keys = new Set(Object.keys(value));
      const problems = [];
      for (const key of forbiddenSet)
        if (check(keys, key, value))
          problems.push(key);
      if (problems.length > 0)
        return pushError(state, `Forbidden ${plural(problems.length, `property`, `properties`)} ${getPrintableArray(problems, `and`)}`);
      return true;
    }
  });
}
function hasMutuallyExclusiveKeys(exclusiveKeys, options) {
  var _a;
  const exclusiveSet = new Set(exclusiveKeys);
  const check = checks[(_a = options === null || options === void 0 ? void 0 : options.missingIf) !== null && _a !== void 0 ? _a : "missing"];
  return makeValidator({
    test: (value, state) => {
      const keys = new Set(Object.keys(value));
      const used = [];
      for (const key of exclusiveSet)
        if (check(keys, key, value))
          used.push(key);
      if (used.length > 1)
        return pushError(state, `Mutually exclusive properties ${getPrintableArray(used, `and`)}`);
      return true;
    }
  });
}
function hasKeyRelationship(subject, relationship, others, options) {
  var _a, _b;
  const skipped = new Set((_a = options === null || options === void 0 ? void 0 : options.ignore) !== null && _a !== void 0 ? _a : []);
  const check = checks[(_b = options === null || options === void 0 ? void 0 : options.missingIf) !== null && _b !== void 0 ? _b : "missing"];
  const otherSet = new Set(others);
  const spec = keyRelationships[relationship];
  const conjunction = relationship === KeyRelationship.Forbids ? `or` : `and`;
  return makeValidator({
    test: (value, state) => {
      const keys = new Set(Object.keys(value));
      if (!check(keys, subject, value) || skipped.has(value[subject]))
        return true;
      const problems = [];
      for (const key of otherSet)
        if ((check(keys, key, value) && !skipped.has(value[key])) !== spec.expect)
          problems.push(key);
      if (problems.length >= 1)
        return pushError(state, `Property "${subject}" ${spec.message} ${plural(problems.length, `property`, `properties`)} ${getPrintableArray(problems, conjunction)}`);
      return true;
    }
  });
}
var simpleKeyRegExp, colorStringRegExp, colorStringAlphaRegExp, base64RegExp, uuid4RegExp, iso8601RegExp, BOOLEAN_COERCIONS, isInstanceOf, isOneOf, TypeAssertionError, checks, KeyRelationship, keyRelationships;
var init_lib = __esm({
  "node_modules/typanion/lib/index.mjs"() {
    simpleKeyRegExp = /^[a-zA-Z_][a-zA-Z0-9_]*$/;
    colorStringRegExp = /^#[0-9a-f]{6}$/i;
    colorStringAlphaRegExp = /^#[0-9a-f]{6}([0-9a-f]{2})?$/i;
    base64RegExp = /^(?:[A-Za-z0-9+/]{4})*(?:[A-Za-z0-9+/]{2}==|[A-Za-z0-9+/]{3}=)?$/;
    uuid4RegExp = /^[a-f0-9]{8}-[a-f0-9]{4}-4[a-f0-9]{3}-[89aAbB][a-f0-9]{3}-[a-f0-9]{12}$/i;
    iso8601RegExp = /^(?:[1-9]\d{3}(-?)(?:(?:0[1-9]|1[0-2])\1(?:0[1-9]|1\d|2[0-8])|(?:0[13-9]|1[0-2])\1(?:29|30)|(?:0[13578]|1[02])(?:\1)31|00[1-9]|0[1-9]\d|[12]\d{2}|3(?:[0-5]\d|6[0-5]))|(?:[1-9]\d(?:0[48]|[2468][048]|[13579][26])|(?:[2468][048]|[13579][26])00)(?:(-?)02(?:\2)29|-?366))T(?:[01]\d|2[0-3])(:?)[0-5]\d(?:\3[0-5]\d)?(?:Z|[+-][01]\d(?:\3[0-5]\d)?)$/;
    BOOLEAN_COERCIONS = /* @__PURE__ */ new Map([
      [`true`, true],
      [`True`, true],
      [`1`, true],
      [1, true],
      [`false`, false],
      [`False`, false],
      [`0`, false],
      [0, false]
    ]);
    isInstanceOf = (constructor) => makeValidator({
      test: (value, state) => {
        if (!(value instanceof constructor))
          return pushError(state, `Expected an instance of ${constructor.name} (got ${getPrintable(value)})`);
        return true;
      }
    });
    isOneOf = (specs, { exclusive = false } = {}) => makeValidator({
      test: (value, state) => {
        var _a, _b, _c;
        const matches = [];
        const errorBuffer = typeof (state === null || state === void 0 ? void 0 : state.errors) !== `undefined` ? [] : void 0;
        for (let t = 0, T = specs.length; t < T; ++t) {
          const subErrors = typeof (state === null || state === void 0 ? void 0 : state.errors) !== `undefined` ? [] : void 0;
          const subCoercions = typeof (state === null || state === void 0 ? void 0 : state.coercions) !== `undefined` ? [] : void 0;
          if (specs[t](value, Object.assign(Object.assign({}, state), { errors: subErrors, coercions: subCoercions, p: `${(_a = state === null || state === void 0 ? void 0 : state.p) !== null && _a !== void 0 ? _a : `.`}#${t + 1}` }))) {
            matches.push([`#${t + 1}`, subCoercions]);
            if (!exclusive) {
              break;
            }
          } else {
            errorBuffer === null || errorBuffer === void 0 ? void 0 : errorBuffer.push(subErrors[0]);
          }
        }
        if (matches.length === 1) {
          const [, subCoercions] = matches[0];
          if (typeof subCoercions !== `undefined`)
            (_b = state === null || state === void 0 ? void 0 : state.coercions) === null || _b === void 0 ? void 0 : _b.push(...subCoercions);
          return true;
        }
        if (matches.length > 1)
          pushError(state, `Expected to match exactly a single predicate (matched ${matches.join(`, `)})`);
        else
          (_c = state === null || state === void 0 ? void 0 : state.errors) === null || _c === void 0 ? void 0 : _c.push(...errorBuffer);
        return false;
      }
    });
    TypeAssertionError = class extends Error {
      constructor({ errors } = {}) {
        let errorMessage = `Type mismatch`;
        if (errors && errors.length > 0) {
          errorMessage += `
`;
          for (const error of errors) {
            errorMessage += `
- ${error}`;
          }
        }
        super(errorMessage);
      }
    };
    checks = {
      missing: (keys, key) => keys.has(key),
      undefined: (keys, key, value) => keys.has(key) && typeof value[key] !== `undefined`,
      nil: (keys, key, value) => keys.has(key) && value[key] != null,
      falsy: (keys, key, value) => keys.has(key) && !!value[key]
    };
    (function(KeyRelationship2) {
      KeyRelationship2["Forbids"] = "Forbids";
      KeyRelationship2["Requires"] = "Requires";
    })(KeyRelationship || (KeyRelationship = {}));
    keyRelationships = {
      [KeyRelationship.Forbids]: {
        expect: false,
        message: `forbids using`
      },
      [KeyRelationship.Requires]: {
        expect: true,
        message: `requires using`
      }
    };
  }
});

// node_modules/clipanion/lib/advanced/Command.js
var require_Command = __commonJS({
  "node_modules/clipanion/lib/advanced/Command.js"(exports2) {
    "use strict";
    Object.defineProperty(exports2, "__esModule", { value: true });
    var advanced_options_utils = require_utils();
    function _interopNamespace(e) {
      if (e && e.__esModule) return e;
      var n = /* @__PURE__ */ Object.create(null);
      if (e) {
        Object.keys(e).forEach(function(k) {
          if (k !== "default") {
            var d = Object.getOwnPropertyDescriptor(e, k);
            Object.defineProperty(n, k, d.get ? d : {
              enumerable: true,
              get: function() {
                return e[k];
              }
            });
          }
        });
      }
      n["default"] = e;
      return Object.freeze(n);
    }
    var Command12 = class {
      constructor() {
        this.help = false;
      }
      /**
       * Defines the usage information for the given command.
       */
      static Usage(usage) {
        return usage;
      }
      /**
       * Standard error handler which will simply rethrow the error. Can be used
       * to add custom logic to handle errors from the command or simply return
       * the parent class error handling.
       */
      async catch(error) {
        throw error;
      }
      async validateAndExecute() {
        const commandClass = this.constructor;
        const cascade2 = commandClass.schema;
        if (Array.isArray(cascade2)) {
          const { isDict: isDict2, isUnknown: isUnknown2, applyCascade: applyCascade2 } = await Promise.resolve().then(function() {
            return /* @__PURE__ */ _interopNamespace((init_lib(), __toCommonJS(lib_exports)));
          });
          const schema = applyCascade2(isDict2(isUnknown2()), cascade2);
          const errors = [];
          const coercions = [];
          const check = schema(this, { errors, coercions });
          if (!check)
            throw advanced_options_utils.formatError(`Invalid option schema`, errors);
          for (const [, op] of coercions) {
            op();
          }
        } else if (cascade2 != null) {
          throw new Error(`Invalid command schema`);
        }
        const exitCode = await this.execute();
        if (typeof exitCode !== `undefined`) {
          return exitCode;
        } else {
          return 0;
        }
      }
    };
    Command12.isOption = advanced_options_utils.isOptionSymbol;
    Command12.Default = [];
    exports2.Command = Command12;
  }
});

// node_modules/clipanion/lib/core.js
var require_core = __commonJS({
  "node_modules/clipanion/lib/core.js"(exports2) {
    "use strict";
    Object.defineProperty(exports2, "__esModule", { value: true });
    var constants2 = require_constants();
    var errors = require_errors();
    function debug(str) {
      if (constants2.DEBUG) {
        console.log(str);
      }
    }
    var basicHelpState = {
      candidateUsage: null,
      requiredOptions: [],
      errorMessage: null,
      ignoreOptions: false,
      path: [],
      positionals: [],
      options: [],
      remainder: null,
      selectedIndex: constants2.HELP_COMMAND_INDEX
    };
    function makeStateMachine() {
      return {
        nodes: [makeNode(), makeNode(), makeNode()]
      };
    }
    function makeAnyOfMachine(inputs) {
      const output = makeStateMachine();
      const heads = [];
      let offset = output.nodes.length;
      for (const input of inputs) {
        heads.push(offset);
        for (let t = 0; t < input.nodes.length; ++t)
          if (!isTerminalNode(t))
            output.nodes.push(cloneNode(input.nodes[t], offset));
        offset += input.nodes.length - 2;
      }
      for (const head of heads)
        registerShortcut(output, constants2.NODE_INITIAL, head);
      return output;
    }
    function injectNode(machine, node) {
      machine.nodes.push(node);
      return machine.nodes.length - 1;
    }
    function simplifyMachine(input) {
      const visited = /* @__PURE__ */ new Set();
      const process5 = (node) => {
        if (visited.has(node))
          return;
        visited.add(node);
        const nodeDef = input.nodes[node];
        for (const transitions of Object.values(nodeDef.statics))
          for (const { to } of transitions)
            process5(to);
        for (const [, { to }] of nodeDef.dynamics)
          process5(to);
        for (const { to } of nodeDef.shortcuts)
          process5(to);
        const shortcuts = new Set(nodeDef.shortcuts.map(({ to }) => to));
        while (nodeDef.shortcuts.length > 0) {
          const { to } = nodeDef.shortcuts.shift();
          const toDef = input.nodes[to];
          for (const [segment, transitions] of Object.entries(toDef.statics)) {
            const store = !Object.prototype.hasOwnProperty.call(nodeDef.statics, segment) ? nodeDef.statics[segment] = [] : nodeDef.statics[segment];
            for (const transition of transitions) {
              if (!store.some(({ to: to2 }) => transition.to === to2)) {
                store.push(transition);
              }
            }
          }
          for (const [test, transition] of toDef.dynamics)
            if (!nodeDef.dynamics.some(([otherTest, { to: to2 }]) => test === otherTest && transition.to === to2))
              nodeDef.dynamics.push([test, transition]);
          for (const transition of toDef.shortcuts) {
            if (!shortcuts.has(transition.to)) {
              nodeDef.shortcuts.push(transition);
              shortcuts.add(transition.to);
            }
          }
        }
      };
      process5(constants2.NODE_INITIAL);
    }
    function debugMachine(machine, { prefix = `` } = {}) {
      if (constants2.DEBUG) {
        debug(`${prefix}Nodes are:`);
        for (let t = 0; t < machine.nodes.length; ++t) {
          debug(`${prefix}  ${t}: ${JSON.stringify(machine.nodes[t])}`);
        }
      }
    }
    function runMachineInternal(machine, input, partial = false) {
      debug(`Running a vm on ${JSON.stringify(input)}`);
      let branches = [{ node: constants2.NODE_INITIAL, state: {
        candidateUsage: null,
        requiredOptions: [],
        errorMessage: null,
        ignoreOptions: false,
        options: [],
        path: [],
        positionals: [],
        remainder: null,
        selectedIndex: null
      } }];
      debugMachine(machine, { prefix: `  ` });
      const tokens = [constants2.START_OF_INPUT, ...input];
      for (let t = 0; t < tokens.length; ++t) {
        const segment = tokens[t];
        debug(`  Processing ${JSON.stringify(segment)}`);
        const nextBranches = [];
        for (const { node, state } of branches) {
          debug(`    Current node is ${node}`);
          const nodeDef = machine.nodes[node];
          if (node === constants2.NODE_ERRORED) {
            nextBranches.push({ node, state });
            continue;
          }
          console.assert(nodeDef.shortcuts.length === 0, `Shortcuts should have been eliminated by now`);
          const hasExactMatch = Object.prototype.hasOwnProperty.call(nodeDef.statics, segment);
          if (!partial || t < tokens.length - 1 || hasExactMatch) {
            if (hasExactMatch) {
              const transitions = nodeDef.statics[segment];
              for (const { to, reducer } of transitions) {
                nextBranches.push({ node: to, state: typeof reducer !== `undefined` ? execute(reducers, reducer, state, segment) : state });
                debug(`      Static transition to ${to} found`);
              }
            } else {
              debug(`      No static transition found`);
            }
          } else {
            let hasMatches = false;
            for (const candidate of Object.keys(nodeDef.statics)) {
              if (!candidate.startsWith(segment))
                continue;
              if (segment === candidate) {
                for (const { to, reducer } of nodeDef.statics[candidate]) {
                  nextBranches.push({ node: to, state: typeof reducer !== `undefined` ? execute(reducers, reducer, state, segment) : state });
                  debug(`      Static transition to ${to} found`);
                }
              } else {
                for (const { to } of nodeDef.statics[candidate]) {
                  nextBranches.push({ node: to, state: { ...state, remainder: candidate.slice(segment.length) } });
                  debug(`      Static transition to ${to} found (partial match)`);
                }
              }
              hasMatches = true;
            }
            if (!hasMatches) {
              debug(`      No partial static transition found`);
            }
          }
          if (segment !== constants2.END_OF_INPUT) {
            for (const [test, { to, reducer }] of nodeDef.dynamics) {
              if (execute(tests, test, state, segment)) {
                nextBranches.push({ node: to, state: typeof reducer !== `undefined` ? execute(reducers, reducer, state, segment) : state });
                debug(`      Dynamic transition to ${to} found (via ${test})`);
              }
            }
          }
        }
        if (nextBranches.length === 0 && segment === constants2.END_OF_INPUT && input.length === 1) {
          return [{
            node: constants2.NODE_INITIAL,
            state: basicHelpState
          }];
        }
        if (nextBranches.length === 0) {
          throw new errors.UnknownSyntaxError(input, branches.filter(({ node }) => {
            return node !== constants2.NODE_ERRORED;
          }).map(({ state }) => {
            return { usage: state.candidateUsage, reason: null };
          }));
        }
        if (nextBranches.every(({ node }) => node === constants2.NODE_ERRORED)) {
          throw new errors.UnknownSyntaxError(input, nextBranches.map(({ state }) => {
            return { usage: state.candidateUsage, reason: state.errorMessage };
          }));
        }
        branches = trimSmallerBranches(nextBranches);
      }
      if (branches.length > 0) {
        debug(`  Results:`);
        for (const branch of branches) {
          debug(`    - ${branch.node} -> ${JSON.stringify(branch.state)}`);
        }
      } else {
        debug(`  No results`);
      }
      return branches;
    }
    function checkIfNodeIsFinished(node, state) {
      if (state.selectedIndex !== null)
        return true;
      if (Object.prototype.hasOwnProperty.call(node.statics, constants2.END_OF_INPUT)) {
        for (const { to } of node.statics[constants2.END_OF_INPUT])
          if (to === constants2.NODE_SUCCESS)
            return true;
      }
      return false;
    }
    function suggestMachine(machine, input, partial) {
      const prefix = partial && input.length > 0 ? [``] : [];
      const branches = runMachineInternal(machine, input, partial);
      const suggestions = [];
      const suggestionsJson = /* @__PURE__ */ new Set();
      const traverseSuggestion = (suggestion, node, skipFirst = true) => {
        let nextNodes = [node];
        while (nextNodes.length > 0) {
          const currentNodes = nextNodes;
          nextNodes = [];
          for (const node2 of currentNodes) {
            const nodeDef = machine.nodes[node2];
            const keys = Object.keys(nodeDef.statics);
            for (const key of Object.keys(nodeDef.statics)) {
              const segment = keys[0];
              for (const { to, reducer } of nodeDef.statics[segment]) {
                if (reducer !== `pushPath`)
                  continue;
                if (!skipFirst)
                  suggestion.push(segment);
                nextNodes.push(to);
              }
            }
          }
          skipFirst = false;
        }
        const json = JSON.stringify(suggestion);
        if (suggestionsJson.has(json))
          return;
        suggestions.push(suggestion);
        suggestionsJson.add(json);
      };
      for (const { node, state } of branches) {
        if (state.remainder !== null) {
          traverseSuggestion([state.remainder], node);
          continue;
        }
        const nodeDef = machine.nodes[node];
        const isFinished = checkIfNodeIsFinished(nodeDef, state);
        for (const [candidate, transitions] of Object.entries(nodeDef.statics))
          if (isFinished && candidate !== constants2.END_OF_INPUT || !candidate.startsWith(`-`) && transitions.some(({ reducer }) => reducer === `pushPath`))
            traverseSuggestion([...prefix, candidate], node);
        if (!isFinished)
          continue;
        for (const [test, { to }] of nodeDef.dynamics) {
          if (to === constants2.NODE_ERRORED)
            continue;
          const tokens = suggest(test, state);
          if (tokens === null)
            continue;
          for (const token of tokens) {
            traverseSuggestion([...prefix, token], node);
          }
        }
      }
      return [...suggestions].sort();
    }
    function runMachine(machine, input) {
      const branches = runMachineInternal(machine, [...input, constants2.END_OF_INPUT]);
      return selectBestState(input, branches.map(({ state }) => {
        return state;
      }));
    }
    function trimSmallerBranches(branches) {
      let maxPathSize = 0;
      for (const { state } of branches)
        if (state.path.length > maxPathSize)
          maxPathSize = state.path.length;
      return branches.filter(({ state }) => {
        return state.path.length === maxPathSize;
      });
    }
    function selectBestState(input, states) {
      const terminalStates = states.filter((state) => {
        return state.selectedIndex !== null;
      });
      if (terminalStates.length === 0)
        throw new Error();
      const requiredOptionsSetStates = terminalStates.filter((state) => state.selectedIndex === constants2.HELP_COMMAND_INDEX || state.requiredOptions.every((names) => names.some((name2) => state.options.find((opt) => opt.name === name2))));
      if (requiredOptionsSetStates.length === 0) {
        throw new errors.UnknownSyntaxError(input, terminalStates.map((state) => ({
          usage: state.candidateUsage,
          reason: null
        })));
      }
      let maxPathSize = 0;
      for (const state of requiredOptionsSetStates)
        if (state.path.length > maxPathSize)
          maxPathSize = state.path.length;
      const bestPathBranches = requiredOptionsSetStates.filter((state) => {
        return state.path.length === maxPathSize;
      });
      const getPositionalCount = (state) => state.positionals.filter(({ extra }) => {
        return !extra;
      }).length + state.options.length;
      const statesWithPositionalCount = bestPathBranches.map((state) => {
        return { state, positionalCount: getPositionalCount(state) };
      });
      let maxPositionalCount = 0;
      for (const { positionalCount } of statesWithPositionalCount)
        if (positionalCount > maxPositionalCount)
          maxPositionalCount = positionalCount;
      const bestPositionalStates = statesWithPositionalCount.filter(({ positionalCount }) => {
        return positionalCount === maxPositionalCount;
      }).map(({ state }) => {
        return state;
      });
      const fixedStates = aggregateHelpStates(bestPositionalStates);
      if (fixedStates.length > 1)
        throw new errors.AmbiguousSyntaxError(input, fixedStates.map((state) => state.candidateUsage));
      return fixedStates[0];
    }
    function aggregateHelpStates(states) {
      const notHelps = [];
      const helps = [];
      for (const state of states) {
        if (state.selectedIndex === constants2.HELP_COMMAND_INDEX) {
          helps.push(state);
        } else {
          notHelps.push(state);
        }
      }
      if (helps.length > 0) {
        notHelps.push({
          ...basicHelpState,
          path: findCommonPrefix(...helps.map((state) => state.path)),
          options: helps.reduce((options, state) => options.concat(state.options), [])
        });
      }
      return notHelps;
    }
    function findCommonPrefix(firstPath, secondPath, ...rest) {
      if (secondPath === void 0)
        return Array.from(firstPath);
      return findCommonPrefix(firstPath.filter((segment, i) => segment === secondPath[i]), ...rest);
    }
    function makeNode() {
      return {
        dynamics: [],
        shortcuts: [],
        statics: {}
      };
    }
    function isTerminalNode(node) {
      return node === constants2.NODE_SUCCESS || node === constants2.NODE_ERRORED;
    }
    function cloneTransition(input, offset = 0) {
      return {
        to: !isTerminalNode(input.to) ? input.to > 2 ? input.to + offset - 2 : input.to + offset : input.to,
        reducer: input.reducer
      };
    }
    function cloneNode(input, offset = 0) {
      const output = makeNode();
      for (const [test, transition] of input.dynamics)
        output.dynamics.push([test, cloneTransition(transition, offset)]);
      for (const transition of input.shortcuts)
        output.shortcuts.push(cloneTransition(transition, offset));
      for (const [segment, transitions] of Object.entries(input.statics))
        output.statics[segment] = transitions.map((transition) => cloneTransition(transition, offset));
      return output;
    }
    function registerDynamic(machine, from, test, to, reducer) {
      machine.nodes[from].dynamics.push([
        test,
        { to, reducer }
      ]);
    }
    function registerShortcut(machine, from, to, reducer) {
      machine.nodes[from].shortcuts.push({ to, reducer });
    }
    function registerStatic(machine, from, test, to, reducer) {
      const store = !Object.prototype.hasOwnProperty.call(machine.nodes[from].statics, test) ? machine.nodes[from].statics[test] = [] : machine.nodes[from].statics[test];
      store.push({ to, reducer });
    }
    function execute(store, callback, state, segment) {
      if (Array.isArray(callback)) {
        const [name2, ...args] = callback;
        return store[name2](state, segment, ...args);
      } else {
        return store[callback](state, segment);
      }
    }
    function suggest(callback, state) {
      const fn2 = Array.isArray(callback) ? tests[callback[0]] : tests[callback];
      if (typeof fn2.suggest === `undefined`)
        return null;
      const args = Array.isArray(callback) ? callback.slice(1) : [];
      return fn2.suggest(state, ...args);
    }
    var tests = {
      always: () => {
        return true;
      },
      isOptionLike: (state, segment) => {
        return !state.ignoreOptions && (segment !== `-` && segment.startsWith(`-`));
      },
      isNotOptionLike: (state, segment) => {
        return state.ignoreOptions || segment === `-` || !segment.startsWith(`-`);
      },
      isOption: (state, segment, name2, hidden) => {
        return !state.ignoreOptions && segment === name2;
      },
      isBatchOption: (state, segment, names) => {
        return !state.ignoreOptions && constants2.BATCH_REGEX.test(segment) && [...segment.slice(1)].every((name2) => names.includes(`-${name2}`));
      },
      isBoundOption: (state, segment, names, options) => {
        const optionParsing = segment.match(constants2.BINDING_REGEX);
        return !state.ignoreOptions && !!optionParsing && constants2.OPTION_REGEX.test(optionParsing[1]) && names.includes(optionParsing[1]) && options.filter((opt) => opt.names.includes(optionParsing[1])).every((opt) => opt.allowBinding);
      },
      isNegatedOption: (state, segment, name2) => {
        return !state.ignoreOptions && segment === `--no-${name2.slice(2)}`;
      },
      isHelp: (state, segment) => {
        return !state.ignoreOptions && constants2.HELP_REGEX.test(segment);
      },
      isUnsupportedOption: (state, segment, names) => {
        return !state.ignoreOptions && segment.startsWith(`-`) && constants2.OPTION_REGEX.test(segment) && !names.includes(segment);
      },
      isInvalidOption: (state, segment) => {
        return !state.ignoreOptions && segment.startsWith(`-`) && !constants2.OPTION_REGEX.test(segment);
      }
    };
    tests.isOption.suggest = (state, name2, hidden = true) => {
      return !hidden ? [name2] : null;
    };
    var reducers = {
      setCandidateState: (state, segment, candidateState) => {
        return { ...state, ...candidateState };
      },
      setSelectedIndex: (state, segment, index) => {
        return { ...state, selectedIndex: index };
      },
      pushBatch: (state, segment) => {
        return { ...state, options: state.options.concat([...segment.slice(1)].map((name2) => ({ name: `-${name2}`, value: true }))) };
      },
      pushBound: (state, segment) => {
        const [, name2, value] = segment.match(constants2.BINDING_REGEX);
        return { ...state, options: state.options.concat({ name: name2, value }) };
      },
      pushPath: (state, segment) => {
        return { ...state, path: state.path.concat(segment) };
      },
      pushPositional: (state, segment) => {
        return { ...state, positionals: state.positionals.concat({ value: segment, extra: false }) };
      },
      pushExtra: (state, segment) => {
        return { ...state, positionals: state.positionals.concat({ value: segment, extra: true }) };
      },
      pushExtraNoLimits: (state, segment) => {
        return { ...state, positionals: state.positionals.concat({ value: segment, extra: NoLimits }) };
      },
      pushTrue: (state, segment, name2 = segment) => {
        return { ...state, options: state.options.concat({ name: segment, value: true }) };
      },
      pushFalse: (state, segment, name2 = segment) => {
        return { ...state, options: state.options.concat({ name: name2, value: false }) };
      },
      pushUndefined: (state, segment) => {
        return { ...state, options: state.options.concat({ name: segment, value: void 0 }) };
      },
      pushStringValue: (state, segment) => {
        var _a;
        const copy = { ...state, options: [...state.options] };
        const lastOption = state.options[state.options.length - 1];
        lastOption.value = ((_a = lastOption.value) !== null && _a !== void 0 ? _a : []).concat([segment]);
        return copy;
      },
      setStringValue: (state, segment) => {
        const copy = { ...state, options: [...state.options] };
        const lastOption = state.options[state.options.length - 1];
        lastOption.value = segment;
        return copy;
      },
      inhibateOptions: (state) => {
        return { ...state, ignoreOptions: true };
      },
      useHelp: (state, segment, command) => {
        const [
          ,
          /* name */
          ,
          index
        ] = segment.match(constants2.HELP_REGEX);
        if (typeof index !== `undefined`) {
          return { ...state, options: [{ name: `-c`, value: String(command) }, { name: `-i`, value: index }] };
        } else {
          return { ...state, options: [{ name: `-c`, value: String(command) }] };
        }
      },
      setError: (state, segment, errorMessage) => {
        if (segment === constants2.END_OF_INPUT) {
          return { ...state, errorMessage: `${errorMessage}.` };
        } else {
          return { ...state, errorMessage: `${errorMessage} ("${segment}").` };
        }
      },
      setOptionArityError: (state, segment) => {
        const lastOption = state.options[state.options.length - 1];
        return { ...state, errorMessage: `Not enough arguments to option ${lastOption.name}.` };
      }
    };
    var NoLimits = /* @__PURE__ */ Symbol();
    var CommandBuilder = class {
      constructor(cliIndex, cliOpts) {
        this.allOptionNames = [];
        this.arity = { leading: [], trailing: [], extra: [], proxy: false };
        this.options = [];
        this.paths = [];
        this.cliIndex = cliIndex;
        this.cliOpts = cliOpts;
      }
      addPath(path16) {
        this.paths.push(path16);
      }
      setArity({ leading = this.arity.leading, trailing = this.arity.trailing, extra = this.arity.extra, proxy = this.arity.proxy }) {
        Object.assign(this.arity, { leading, trailing, extra, proxy });
      }
      addPositional({ name: name2 = `arg`, required = true } = {}) {
        if (!required && this.arity.extra === NoLimits)
          throw new Error(`Optional parameters cannot be declared when using .rest() or .proxy()`);
        if (!required && this.arity.trailing.length > 0)
          throw new Error(`Optional parameters cannot be declared after the required trailing positional arguments`);
        if (!required && this.arity.extra !== NoLimits) {
          this.arity.extra.push(name2);
        } else if (this.arity.extra !== NoLimits && this.arity.extra.length === 0) {
          this.arity.leading.push(name2);
        } else {
          this.arity.trailing.push(name2);
        }
      }
      addRest({ name: name2 = `arg`, required = 0 } = {}) {
        if (this.arity.extra === NoLimits)
          throw new Error(`Infinite lists cannot be declared multiple times in the same command`);
        if (this.arity.trailing.length > 0)
          throw new Error(`Infinite lists cannot be declared after the required trailing positional arguments`);
        for (let t = 0; t < required; ++t)
          this.addPositional({ name: name2 });
        this.arity.extra = NoLimits;
      }
      addProxy({ required = 0 } = {}) {
        this.addRest({ required });
        this.arity.proxy = true;
      }
      addOption({ names, description, arity = 0, hidden = false, required = false, allowBinding = true }) {
        if (!allowBinding && arity > 1)
          throw new Error(`The arity cannot be higher than 1 when the option only supports the --arg=value syntax`);
        if (!Number.isInteger(arity))
          throw new Error(`The arity must be an integer, got ${arity}`);
        if (arity < 0)
          throw new Error(`The arity must be positive, got ${arity}`);
        this.allOptionNames.push(...names);
        this.options.push({ names, description, arity, hidden, required, allowBinding });
      }
      setContext(context) {
        this.context = context;
      }
      usage({ detailed = true, inlineOptions = true } = {}) {
        const segments = [this.cliOpts.binaryName];
        const detailedOptionList = [];
        if (this.paths.length > 0)
          segments.push(...this.paths[0]);
        if (detailed) {
          for (const { names, arity, hidden, description, required } of this.options) {
            if (hidden)
              continue;
            const args = [];
            for (let t = 0; t < arity; ++t)
              args.push(` #${t}`);
            const definition = `${names.join(`,`)}${args.join(``)}`;
            if (!inlineOptions && description) {
              detailedOptionList.push({ definition, description, required });
            } else {
              segments.push(required ? `<${definition}>` : `[${definition}]`);
            }
          }
          segments.push(...this.arity.leading.map((name2) => `<${name2}>`));
          if (this.arity.extra === NoLimits)
            segments.push(`...`);
          else
            segments.push(...this.arity.extra.map((name2) => `[${name2}]`));
          segments.push(...this.arity.trailing.map((name2) => `<${name2}>`));
        }
        const usage = segments.join(` `);
        return { usage, options: detailedOptionList };
      }
      compile() {
        if (typeof this.context === `undefined`)
          throw new Error(`Assertion failed: No context attached`);
        const machine = makeStateMachine();
        let firstNode = constants2.NODE_INITIAL;
        const candidateUsage = this.usage().usage;
        const requiredOptions = this.options.filter((opt) => opt.required).map((opt) => opt.names);
        firstNode = injectNode(machine, makeNode());
        registerStatic(machine, constants2.NODE_INITIAL, constants2.START_OF_INPUT, firstNode, [`setCandidateState`, { candidateUsage, requiredOptions }]);
        const positionalArgument = this.arity.proxy ? `always` : `isNotOptionLike`;
        const paths = this.paths.length > 0 ? this.paths : [[]];
        for (const path16 of paths) {
          let lastPathNode = firstNode;
          if (path16.length > 0) {
            const optionPathNode = injectNode(machine, makeNode());
            registerShortcut(machine, lastPathNode, optionPathNode);
            this.registerOptions(machine, optionPathNode);
            lastPathNode = optionPathNode;
          }
          for (let t = 0; t < path16.length; ++t) {
            const nextPathNode = injectNode(machine, makeNode());
            registerStatic(machine, lastPathNode, path16[t], nextPathNode, `pushPath`);
            lastPathNode = nextPathNode;
          }
          if (this.arity.leading.length > 0 || !this.arity.proxy) {
            const helpNode = injectNode(machine, makeNode());
            registerDynamic(machine, lastPathNode, `isHelp`, helpNode, [`useHelp`, this.cliIndex]);
            registerDynamic(machine, helpNode, `always`, helpNode, `pushExtra`);
            registerStatic(machine, helpNode, constants2.END_OF_INPUT, constants2.NODE_SUCCESS, [`setSelectedIndex`, constants2.HELP_COMMAND_INDEX]);
            this.registerOptions(machine, lastPathNode);
          }
          if (this.arity.leading.length > 0)
            registerStatic(machine, lastPathNode, constants2.END_OF_INPUT, constants2.NODE_ERRORED, [`setError`, `Not enough positional arguments`]);
          let lastLeadingNode = lastPathNode;
          for (let t = 0; t < this.arity.leading.length; ++t) {
            const nextLeadingNode = injectNode(machine, makeNode());
            if (!this.arity.proxy || t + 1 !== this.arity.leading.length)
              this.registerOptions(machine, nextLeadingNode);
            if (this.arity.trailing.length > 0 || t + 1 !== this.arity.leading.length)
              registerStatic(machine, nextLeadingNode, constants2.END_OF_INPUT, constants2.NODE_ERRORED, [`setError`, `Not enough positional arguments`]);
            registerDynamic(machine, lastLeadingNode, `isNotOptionLike`, nextLeadingNode, `pushPositional`);
            lastLeadingNode = nextLeadingNode;
          }
          let lastExtraNode = lastLeadingNode;
          if (this.arity.extra === NoLimits || this.arity.extra.length > 0) {
            const extraShortcutNode = injectNode(machine, makeNode());
            registerShortcut(machine, lastLeadingNode, extraShortcutNode);
            if (this.arity.extra === NoLimits) {
              const extraNode = injectNode(machine, makeNode());
              if (!this.arity.proxy)
                this.registerOptions(machine, extraNode);
              registerDynamic(machine, lastLeadingNode, positionalArgument, extraNode, `pushExtraNoLimits`);
              registerDynamic(machine, extraNode, positionalArgument, extraNode, `pushExtraNoLimits`);
              registerShortcut(machine, extraNode, extraShortcutNode);
            } else {
              for (let t = 0; t < this.arity.extra.length; ++t) {
                const nextExtraNode = injectNode(machine, makeNode());
                if (!this.arity.proxy || t > 0)
                  this.registerOptions(machine, nextExtraNode);
                registerDynamic(machine, lastExtraNode, positionalArgument, nextExtraNode, `pushExtra`);
                registerShortcut(machine, nextExtraNode, extraShortcutNode);
                lastExtraNode = nextExtraNode;
              }
            }
            lastExtraNode = extraShortcutNode;
          }
          if (this.arity.trailing.length > 0)
            registerStatic(machine, lastExtraNode, constants2.END_OF_INPUT, constants2.NODE_ERRORED, [`setError`, `Not enough positional arguments`]);
          let lastTrailingNode = lastExtraNode;
          for (let t = 0; t < this.arity.trailing.length; ++t) {
            const nextTrailingNode = injectNode(machine, makeNode());
            if (!this.arity.proxy)
              this.registerOptions(machine, nextTrailingNode);
            if (t + 1 < this.arity.trailing.length)
              registerStatic(machine, nextTrailingNode, constants2.END_OF_INPUT, constants2.NODE_ERRORED, [`setError`, `Not enough positional arguments`]);
            registerDynamic(machine, lastTrailingNode, `isNotOptionLike`, nextTrailingNode, `pushPositional`);
            lastTrailingNode = nextTrailingNode;
          }
          registerDynamic(machine, lastTrailingNode, positionalArgument, constants2.NODE_ERRORED, [`setError`, `Extraneous positional argument`]);
          registerStatic(machine, lastTrailingNode, constants2.END_OF_INPUT, constants2.NODE_SUCCESS, [`setSelectedIndex`, this.cliIndex]);
        }
        return {
          machine,
          context: this.context
        };
      }
      registerOptions(machine, node) {
        registerDynamic(machine, node, [`isOption`, `--`], node, `inhibateOptions`);
        registerDynamic(machine, node, [`isBatchOption`, this.allOptionNames], node, `pushBatch`);
        registerDynamic(machine, node, [`isBoundOption`, this.allOptionNames, this.options], node, `pushBound`);
        registerDynamic(machine, node, [`isUnsupportedOption`, this.allOptionNames], constants2.NODE_ERRORED, [`setError`, `Unsupported option name`]);
        registerDynamic(machine, node, [`isInvalidOption`], constants2.NODE_ERRORED, [`setError`, `Invalid option name`]);
        for (const option of this.options) {
          const longestName = option.names.reduce((longestName2, name2) => {
            return name2.length > longestName2.length ? name2 : longestName2;
          }, ``);
          if (option.arity === 0) {
            for (const name2 of option.names) {
              registerDynamic(machine, node, [`isOption`, name2, option.hidden || name2 !== longestName], node, `pushTrue`);
              if (name2.startsWith(`--`) && !name2.startsWith(`--no-`)) {
                registerDynamic(machine, node, [`isNegatedOption`, name2], node, [`pushFalse`, name2]);
              }
            }
          } else {
            let lastNode = injectNode(machine, makeNode());
            for (const name2 of option.names)
              registerDynamic(machine, node, [`isOption`, name2, option.hidden || name2 !== longestName], lastNode, `pushUndefined`);
            for (let t = 0; t < option.arity; ++t) {
              const nextNode = injectNode(machine, makeNode());
              registerStatic(machine, lastNode, constants2.END_OF_INPUT, constants2.NODE_ERRORED, `setOptionArityError`);
              registerDynamic(machine, lastNode, `isOptionLike`, constants2.NODE_ERRORED, `setOptionArityError`);
              const action = option.arity === 1 ? `setStringValue` : `pushStringValue`;
              registerDynamic(machine, lastNode, `isNotOptionLike`, nextNode, action);
              lastNode = nextNode;
            }
            registerShortcut(machine, lastNode, node);
          }
        }
      }
    };
    var CliBuilder = class _CliBuilder {
      constructor({ binaryName = `...` } = {}) {
        this.builders = [];
        this.opts = { binaryName };
      }
      static build(cbs, opts = {}) {
        return new _CliBuilder(opts).commands(cbs).compile();
      }
      getBuilderByIndex(n) {
        if (!(n >= 0 && n < this.builders.length))
          throw new Error(`Assertion failed: Out-of-bound command index (${n})`);
        return this.builders[n];
      }
      commands(cbs) {
        for (const cb of cbs)
          cb(this.command());
        return this;
      }
      command() {
        const builder = new CommandBuilder(this.builders.length, this.opts);
        this.builders.push(builder);
        return builder;
      }
      compile() {
        const machines = [];
        const contexts = [];
        for (const builder of this.builders) {
          const { machine: machine2, context } = builder.compile();
          machines.push(machine2);
          contexts.push(context);
        }
        const machine = makeAnyOfMachine(machines);
        simplifyMachine(machine);
        return {
          machine,
          contexts,
          process: (input) => {
            return runMachine(machine, input);
          },
          suggest: (input, partial) => {
            return suggestMachine(machine, input, partial);
          }
        };
      }
    };
    exports2.CliBuilder = CliBuilder;
    exports2.CommandBuilder = CommandBuilder;
    exports2.NoLimits = NoLimits;
    exports2.aggregateHelpStates = aggregateHelpStates;
    exports2.cloneNode = cloneNode;
    exports2.cloneTransition = cloneTransition;
    exports2.debug = debug;
    exports2.debugMachine = debugMachine;
    exports2.execute = execute;
    exports2.injectNode = injectNode;
    exports2.isTerminalNode = isTerminalNode;
    exports2.makeAnyOfMachine = makeAnyOfMachine;
    exports2.makeNode = makeNode;
    exports2.makeStateMachine = makeStateMachine;
    exports2.reducers = reducers;
    exports2.registerDynamic = registerDynamic;
    exports2.registerShortcut = registerShortcut;
    exports2.registerStatic = registerStatic;
    exports2.runMachineInternal = runMachineInternal;
    exports2.selectBestState = selectBestState;
    exports2.simplifyMachine = simplifyMachine;
    exports2.suggest = suggest;
    exports2.tests = tests;
    exports2.trimSmallerBranches = trimSmallerBranches;
  }
});

// node_modules/clipanion/lib/platform/node.mjs
var node_exports = {};
__export(node_exports, {
  getCaptureActivator: () => getCaptureActivator,
  getDefaultColorDepth: () => getDefaultColorDepth
});
function getDefaultColorDepth() {
  if (import_tty.default && `getColorDepth` in import_tty.default.WriteStream.prototype)
    return import_tty.default.WriteStream.prototype.getColorDepth();
  if (process.env.FORCE_COLOR === `0`)
    return 1;
  if (process.env.FORCE_COLOR === `1`)
    return 8;
  if (typeof process.stdout !== `undefined` && process.stdout.isTTY)
    return 8;
  return 1;
}
function getCaptureActivator(context) {
  let contextStorage = gContextStorage;
  if (typeof contextStorage === `undefined`) {
    if (context.stdout === process.stdout && context.stderr === process.stderr)
      return null;
    const { AsyncLocalStorage: LazyAsyncLocalStorage } = require("async_hooks");
    contextStorage = gContextStorage = new LazyAsyncLocalStorage();
    const origStdoutWrite = process.stdout._write;
    process.stdout._write = function(chunk, encoding, cb) {
      const context2 = contextStorage.getStore();
      if (typeof context2 === `undefined`)
        return origStdoutWrite.call(this, chunk, encoding, cb);
      return context2.stdout.write(chunk, encoding, cb);
    };
    const origStderrWrite = process.stderr._write;
    process.stderr._write = function(chunk, encoding, cb) {
      const context2 = contextStorage.getStore();
      if (typeof context2 === `undefined`)
        return origStderrWrite.call(this, chunk, encoding, cb);
      return context2.stderr.write(chunk, encoding, cb);
    };
  }
  return (fn2) => {
    return contextStorage.run(context, fn2);
  };
}
var import_tty, gContextStorage;
var init_node = __esm({
  "node_modules/clipanion/lib/platform/node.mjs"() {
    import_tty = __toESM(require("tty"), 1);
  }
});

// node_modules/clipanion/lib/advanced/HelpCommand.js
var require_HelpCommand = __commonJS({
  "node_modules/clipanion/lib/advanced/HelpCommand.js"(exports2) {
    "use strict";
    Object.defineProperty(exports2, "__esModule", { value: true });
    var advanced_Command = require_Command();
    var HelpCommand = class _HelpCommand extends advanced_Command.Command {
      constructor(contexts) {
        super();
        this.contexts = contexts;
        this.commands = [];
      }
      static from(state, contexts) {
        const command = new _HelpCommand(contexts);
        command.path = state.path;
        for (const opt of state.options) {
          switch (opt.name) {
            case `-c`:
              {
                command.commands.push(Number(opt.value));
              }
              break;
            case `-i`:
              {
                command.index = Number(opt.value);
              }
              break;
          }
        }
        return command;
      }
      async execute() {
        let commands = this.commands;
        if (typeof this.index !== `undefined` && this.index >= 0 && this.index < commands.length)
          commands = [commands[this.index]];
        if (commands.length === 0) {
          this.context.stdout.write(this.cli.usage());
        } else if (commands.length === 1) {
          this.context.stdout.write(this.cli.usage(this.contexts[commands[0]].commandClass, { detailed: true }));
        } else if (commands.length > 1) {
          this.context.stdout.write(`Multiple commands match your selection:
`);
          this.context.stdout.write(`
`);
          let index = 0;
          for (const command of this.commands)
            this.context.stdout.write(this.cli.usage(this.contexts[command].commandClass, { prefix: `${index++}. `.padStart(5) }));
          this.context.stdout.write(`
`);
          this.context.stdout.write(`Run again with -h=<index> to see the longer details of any of those commands.
`);
        }
      }
    };
    exports2.HelpCommand = HelpCommand;
  }
});

// node_modules/clipanion/lib/advanced/Cli.js
var require_Cli = __commonJS({
  "node_modules/clipanion/lib/advanced/Cli.js"(exports2) {
    "use strict";
    Object.defineProperty(exports2, "__esModule", { value: true });
    var constants2 = require_constants();
    var core = require_core();
    var format = require_format();
    var platform5 = (init_node(), __toCommonJS(node_exports));
    var advanced_Command = require_Command();
    var advanced_HelpCommand = require_HelpCommand();
    var errorCommandSymbol = /* @__PURE__ */ Symbol(`clipanion/errorCommand`);
    async function runExit(...args) {
      const { resolvedOptions, resolvedCommandClasses, resolvedArgv, resolvedContext } = resolveRunParameters(args);
      const cli = Cli2.from(resolvedCommandClasses, resolvedOptions);
      return cli.runExit(resolvedArgv, resolvedContext);
    }
    async function run(...args) {
      const { resolvedOptions, resolvedCommandClasses, resolvedArgv, resolvedContext } = resolveRunParameters(args);
      const cli = Cli2.from(resolvedCommandClasses, resolvedOptions);
      return cli.run(resolvedArgv, resolvedContext);
    }
    function resolveRunParameters(args) {
      let resolvedOptions;
      let resolvedCommandClasses;
      let resolvedArgv;
      let resolvedContext;
      if (typeof process !== `undefined` && typeof process.argv !== `undefined`)
        resolvedArgv = process.argv.slice(2);
      switch (args.length) {
        case 1:
          {
            resolvedCommandClasses = args[0];
          }
          break;
        case 2:
          {
            if (args[0] && args[0].prototype instanceof advanced_Command.Command || Array.isArray(args[0])) {
              resolvedCommandClasses = args[0];
              if (Array.isArray(args[1])) {
                resolvedArgv = args[1];
              } else {
                resolvedContext = args[1];
              }
            } else {
              resolvedOptions = args[0];
              resolvedCommandClasses = args[1];
            }
          }
          break;
        case 3:
          {
            if (Array.isArray(args[2])) {
              resolvedOptions = args[0];
              resolvedCommandClasses = args[1];
              resolvedArgv = args[2];
            } else if (args[0] && args[0].prototype instanceof advanced_Command.Command || Array.isArray(args[0])) {
              resolvedCommandClasses = args[0];
              resolvedArgv = args[1];
              resolvedContext = args[2];
            } else {
              resolvedOptions = args[0];
              resolvedCommandClasses = args[1];
              resolvedContext = args[2];
            }
          }
          break;
        default:
          {
            resolvedOptions = args[0];
            resolvedCommandClasses = args[1];
            resolvedArgv = args[2];
            resolvedContext = args[3];
          }
          break;
      }
      if (typeof resolvedArgv === `undefined`)
        throw new Error(`The argv parameter must be provided when running Clipanion outside of a Node context`);
      return {
        resolvedOptions,
        resolvedCommandClasses,
        resolvedArgv,
        resolvedContext
      };
    }
    var Cli2 = class _Cli {
      constructor({ binaryLabel, binaryName: binaryNameOpt = `...`, binaryVersion, enableCapture = false, enableColors } = {}) {
        this.registrations = /* @__PURE__ */ new Map();
        this.builder = new core.CliBuilder({ binaryName: binaryNameOpt });
        this.binaryLabel = binaryLabel;
        this.binaryName = binaryNameOpt;
        this.binaryVersion = binaryVersion;
        this.enableCapture = enableCapture;
        this.enableColors = enableColors;
      }
      /**
       * Creates a new Cli and registers all commands passed as parameters.
       *
       * @param commandClasses The Commands to register
       * @returns The created `Cli` instance
       */
      static from(commandClasses, options = {}) {
        const cli = new _Cli(options);
        const resolvedCommandClasses = Array.isArray(commandClasses) ? commandClasses : [commandClasses];
        for (const commandClass of resolvedCommandClasses)
          cli.register(commandClass);
        return cli;
      }
      /**
       * Registers a command inside the CLI.
       */
      register(commandClass) {
        var _a;
        const specs = /* @__PURE__ */ new Map();
        const command = new commandClass();
        for (const key in command) {
          const value = command[key];
          if (typeof value === `object` && value !== null && value[advanced_Command.Command.isOption]) {
            specs.set(key, value);
          }
        }
        const builder = this.builder.command();
        const index = builder.cliIndex;
        const paths = (_a = commandClass.paths) !== null && _a !== void 0 ? _a : command.paths;
        if (typeof paths !== `undefined`)
          for (const path16 of paths)
            builder.addPath(path16);
        this.registrations.set(commandClass, { specs, builder, index });
        for (const [key, { definition }] of specs.entries())
          definition(builder, key);
        builder.setContext({
          commandClass
        });
      }
      process(input, userContext) {
        const { contexts, process: process5 } = this.builder.compile();
        const state = process5(input);
        const context = {
          ..._Cli.defaultContext,
          ...userContext
        };
        switch (state.selectedIndex) {
          case constants2.HELP_COMMAND_INDEX: {
            const command = advanced_HelpCommand.HelpCommand.from(state, contexts);
            command.context = context;
            return command;
          }
          default:
            {
              const { commandClass } = contexts[state.selectedIndex];
              const record = this.registrations.get(commandClass);
              if (typeof record === `undefined`)
                throw new Error(`Assertion failed: Expected the command class to have been registered.`);
              const command = new commandClass();
              command.context = context;
              command.path = state.path;
              try {
                for (const [key, { transformer }] of record.specs.entries())
                  command[key] = transformer(record.builder, key, state, context);
                return command;
              } catch (error) {
                error[errorCommandSymbol] = command;
                throw error;
              }
            }
            break;
        }
      }
      async run(input, userContext) {
        var _a, _b;
        let command;
        const context = {
          ..._Cli.defaultContext,
          ...userContext
        };
        const colored = (_a = this.enableColors) !== null && _a !== void 0 ? _a : context.colorDepth > 1;
        if (!Array.isArray(input)) {
          command = input;
        } else {
          try {
            command = this.process(input, context);
          } catch (error) {
            context.stdout.write(this.error(error, { colored }));
            return 1;
          }
        }
        if (command.help) {
          context.stdout.write(this.usage(command, { colored, detailed: true }));
          return 0;
        }
        command.context = context;
        command.cli = {
          binaryLabel: this.binaryLabel,
          binaryName: this.binaryName,
          binaryVersion: this.binaryVersion,
          enableCapture: this.enableCapture,
          enableColors: this.enableColors,
          definitions: () => this.definitions(),
          error: (error, opts) => this.error(error, opts),
          format: (colored2) => this.format(colored2),
          process: (input2, subContext) => this.process(input2, { ...context, ...subContext }),
          run: (input2, subContext) => this.run(input2, { ...context, ...subContext }),
          usage: (command2, opts) => this.usage(command2, opts)
        };
        const activate = this.enableCapture ? (_b = platform5.getCaptureActivator(context)) !== null && _b !== void 0 ? _b : noopCaptureActivator : noopCaptureActivator;
        let exitCode;
        try {
          exitCode = await activate(() => command.validateAndExecute().catch((error) => command.catch(error).then(() => 0)));
        } catch (error) {
          context.stdout.write(this.error(error, { colored, command }));
          return 1;
        }
        return exitCode;
      }
      async runExit(input, context) {
        process.exitCode = await this.run(input, context);
      }
      suggest(input, partial) {
        const { suggest } = this.builder.compile();
        return suggest(input, partial);
      }
      definitions({ colored = false } = {}) {
        const data = [];
        for (const [commandClass, { index }] of this.registrations) {
          if (typeof commandClass.usage === `undefined`)
            continue;
          const { usage: path16 } = this.getUsageByIndex(index, { detailed: false });
          const { usage, options } = this.getUsageByIndex(index, { detailed: true, inlineOptions: false });
          const category = typeof commandClass.usage.category !== `undefined` ? format.formatMarkdownish(commandClass.usage.category, { format: this.format(colored), paragraphs: false }) : void 0;
          const description = typeof commandClass.usage.description !== `undefined` ? format.formatMarkdownish(commandClass.usage.description, { format: this.format(colored), paragraphs: false }) : void 0;
          const details = typeof commandClass.usage.details !== `undefined` ? format.formatMarkdownish(commandClass.usage.details, { format: this.format(colored), paragraphs: true }) : void 0;
          const examples = typeof commandClass.usage.examples !== `undefined` ? commandClass.usage.examples.map(([label, cli]) => [format.formatMarkdownish(label, { format: this.format(colored), paragraphs: false }), cli.replace(/\$0/g, this.binaryName)]) : void 0;
          data.push({ path: path16, usage, category, description, details, examples, options });
        }
        return data;
      }
      usage(command = null, { colored, detailed = false, prefix = `$ ` } = {}) {
        var _a;
        if (command === null) {
          for (const commandClass2 of this.registrations.keys()) {
            const paths = commandClass2.paths;
            const isDocumented = typeof commandClass2.usage !== `undefined`;
            const isExclusivelyDefault = !paths || paths.length === 0 || paths.length === 1 && paths[0].length === 0;
            const isDefault = isExclusivelyDefault || ((_a = paths === null || paths === void 0 ? void 0 : paths.some((path16) => path16.length === 0)) !== null && _a !== void 0 ? _a : false);
            if (isDefault) {
              if (command) {
                command = null;
                break;
              } else {
                command = commandClass2;
              }
            } else {
              if (isDocumented) {
                command = null;
                continue;
              }
            }
          }
          if (command) {
            detailed = true;
          }
        }
        const commandClass = command !== null && command instanceof advanced_Command.Command ? command.constructor : command;
        let result = ``;
        if (!commandClass) {
          const commandsByCategories = /* @__PURE__ */ new Map();
          for (const [commandClass2, { index }] of this.registrations.entries()) {
            if (typeof commandClass2.usage === `undefined`)
              continue;
            const category = typeof commandClass2.usage.category !== `undefined` ? format.formatMarkdownish(commandClass2.usage.category, { format: this.format(colored), paragraphs: false }) : null;
            let categoryCommands = commandsByCategories.get(category);
            if (typeof categoryCommands === `undefined`)
              commandsByCategories.set(category, categoryCommands = []);
            const { usage } = this.getUsageByIndex(index);
            categoryCommands.push({ commandClass: commandClass2, usage });
          }
          const categoryNames = Array.from(commandsByCategories.keys()).sort((a, b) => {
            if (a === null)
              return -1;
            if (b === null)
              return 1;
            return a.localeCompare(b, `en`, { usage: `sort`, caseFirst: `upper` });
          });
          const hasLabel = typeof this.binaryLabel !== `undefined`;
          const hasVersion = typeof this.binaryVersion !== `undefined`;
          if (hasLabel || hasVersion) {
            if (hasLabel && hasVersion)
              result += `${this.format(colored).header(`${this.binaryLabel} - ${this.binaryVersion}`)}

`;
            else if (hasLabel)
              result += `${this.format(colored).header(`${this.binaryLabel}`)}
`;
            else
              result += `${this.format(colored).header(`${this.binaryVersion}`)}
`;
            result += `  ${this.format(colored).bold(prefix)}${this.binaryName} <command>
`;
          } else {
            result += `${this.format(colored).bold(prefix)}${this.binaryName} <command>
`;
          }
          for (const categoryName of categoryNames) {
            const commands = commandsByCategories.get(categoryName).slice().sort((a, b) => {
              return a.usage.localeCompare(b.usage, `en`, { usage: `sort`, caseFirst: `upper` });
            });
            const header = categoryName !== null ? categoryName.trim() : `General commands`;
            result += `
`;
            result += `${this.format(colored).header(`${header}`)}
`;
            for (const { commandClass: commandClass2, usage } of commands) {
              const doc = commandClass2.usage.description || `undocumented`;
              result += `
`;
              result += `  ${this.format(colored).bold(usage)}
`;
              result += `    ${format.formatMarkdownish(doc, { format: this.format(colored), paragraphs: false })}`;
            }
          }
          result += `
`;
          result += format.formatMarkdownish(`You can also print more details about any of these commands by calling them with the \`-h,--help\` flag right after the command name.`, { format: this.format(colored), paragraphs: true });
        } else {
          if (!detailed) {
            const { usage } = this.getUsageByRegistration(commandClass);
            result += `${this.format(colored).bold(prefix)}${usage}
`;
          } else {
            const { description = ``, details = ``, examples = [] } = commandClass.usage || {};
            if (description !== ``) {
              result += format.formatMarkdownish(description, { format: this.format(colored), paragraphs: false }).replace(/^./, ($0) => $0.toUpperCase());
              result += `
`;
            }
            if (details !== `` || examples.length > 0) {
              result += `${this.format(colored).header(`Usage`)}
`;
              result += `
`;
            }
            const { usage, options } = this.getUsageByRegistration(commandClass, { inlineOptions: false });
            result += `${this.format(colored).bold(prefix)}${usage}
`;
            if (options.length > 0) {
              result += `
`;
              result += `${this.format(colored).header(`Options`)}
`;
              const maxDefinitionLength = options.reduce((length, option) => {
                return Math.max(length, option.definition.length);
              }, 0);
              result += `
`;
              for (const { definition, description: description2 } of options) {
                result += `  ${this.format(colored).bold(definition.padEnd(maxDefinitionLength))}    ${format.formatMarkdownish(description2, { format: this.format(colored), paragraphs: false })}`;
              }
            }
            if (details !== ``) {
              result += `
`;
              result += `${this.format(colored).header(`Details`)}
`;
              result += `
`;
              result += format.formatMarkdownish(details, { format: this.format(colored), paragraphs: true });
            }
            if (examples.length > 0) {
              result += `
`;
              result += `${this.format(colored).header(`Examples`)}
`;
              for (const [description2, example] of examples) {
                result += `
`;
                result += format.formatMarkdownish(description2, { format: this.format(colored), paragraphs: false });
                result += `${example.replace(/^/m, `  ${this.format(colored).bold(prefix)}`).replace(/\$0/g, this.binaryName)}
`;
              }
            }
          }
        }
        return result;
      }
      error(error, _a) {
        var _b;
        var { colored, command = (_b = error[errorCommandSymbol]) !== null && _b !== void 0 ? _b : null } = _a === void 0 ? {} : _a;
        if (!error || typeof error !== `object` || !(`stack` in error))
          error = new Error(`Execution failed with a non-error rejection (rejected value: ${JSON.stringify(error)})`);
        let result = ``;
        let name2 = error.name.replace(/([a-z])([A-Z])/g, `$1 $2`);
        if (name2 === `Error`)
          name2 = `Internal Error`;
        result += `${this.format(colored).error(name2)}: ${error.message}
`;
        const meta = error.clipanion;
        if (typeof meta !== `undefined`) {
          if (meta.type === `usage`) {
            result += `
`;
            result += this.usage(command);
          }
        } else {
          if (error.stack) {
            result += `${error.stack.replace(/^.*\n/, ``)}
`;
          }
        }
        return result;
      }
      format(colored) {
        var _a;
        return ((_a = colored !== null && colored !== void 0 ? colored : this.enableColors) !== null && _a !== void 0 ? _a : _Cli.defaultContext.colorDepth > 1) ? format.richFormat : format.textFormat;
      }
      getUsageByRegistration(klass, opts) {
        const record = this.registrations.get(klass);
        if (typeof record === `undefined`)
          throw new Error(`Assertion failed: Unregistered command`);
        return this.getUsageByIndex(record.index, opts);
      }
      getUsageByIndex(n, opts) {
        return this.builder.getBuilderByIndex(n).usage(opts);
      }
    };
    Cli2.defaultContext = {
      env: process.env,
      stdin: process.stdin,
      stdout: process.stdout,
      stderr: process.stderr,
      colorDepth: platform5.getDefaultColorDepth()
    };
    function noopCaptureActivator(fn2) {
      return fn2();
    }
    exports2.Cli = Cli2;
    exports2.run = run;
    exports2.runExit = runExit;
  }
});

// node_modules/clipanion/lib/advanced/builtins/definitions.js
var require_definitions = __commonJS({
  "node_modules/clipanion/lib/advanced/builtins/definitions.js"(exports2) {
    "use strict";
    Object.defineProperty(exports2, "__esModule", { value: true });
    var advanced_Command = require_Command();
    var DefinitionsCommand = class extends advanced_Command.Command {
      async execute() {
        this.context.stdout.write(`${JSON.stringify(this.cli.definitions(), null, 2)}
`);
      }
    };
    DefinitionsCommand.paths = [[`--clipanion=definitions`]];
    exports2.DefinitionsCommand = DefinitionsCommand;
  }
});

// node_modules/clipanion/lib/advanced/builtins/help.js
var require_help = __commonJS({
  "node_modules/clipanion/lib/advanced/builtins/help.js"(exports2) {
    "use strict";
    Object.defineProperty(exports2, "__esModule", { value: true });
    var advanced_Command = require_Command();
    var HelpCommand = class extends advanced_Command.Command {
      async execute() {
        this.context.stdout.write(this.cli.usage());
      }
    };
    HelpCommand.paths = [[`-h`], [`--help`]];
    exports2.HelpCommand = HelpCommand;
  }
});

// node_modules/clipanion/lib/advanced/builtins/version.js
var require_version = __commonJS({
  "node_modules/clipanion/lib/advanced/builtins/version.js"(exports2) {
    "use strict";
    Object.defineProperty(exports2, "__esModule", { value: true });
    var advanced_Command = require_Command();
    var VersionCommand = class extends advanced_Command.Command {
      async execute() {
        var _a;
        this.context.stdout.write(`${(_a = this.cli.binaryVersion) !== null && _a !== void 0 ? _a : `<unknown>`}
`);
      }
    };
    VersionCommand.paths = [[`-v`], [`--version`]];
    exports2.VersionCommand = VersionCommand;
  }
});

// node_modules/clipanion/lib/advanced/builtins/index.js
var require_builtins = __commonJS({
  "node_modules/clipanion/lib/advanced/builtins/index.js"(exports2) {
    "use strict";
    Object.defineProperty(exports2, "__esModule", { value: true });
    var advanced_builtins_definitions = require_definitions();
    var advanced_builtins_help = require_help();
    var advanced_builtins_version = require_version();
    exports2.DefinitionsCommand = advanced_builtins_definitions.DefinitionsCommand;
    exports2.HelpCommand = advanced_builtins_help.HelpCommand;
    exports2.VersionCommand = advanced_builtins_version.VersionCommand;
  }
});

// node_modules/clipanion/lib/advanced/options/Array.js
var require_Array = __commonJS({
  "node_modules/clipanion/lib/advanced/options/Array.js"(exports2) {
    "use strict";
    Object.defineProperty(exports2, "__esModule", { value: true });
    var advanced_options_utils = require_utils();
    function Array2(descriptor, initialValueBase, optsBase) {
      const [initialValue, opts] = advanced_options_utils.rerouteArguments(initialValueBase, optsBase !== null && optsBase !== void 0 ? optsBase : {});
      const { arity = 1 } = opts;
      const optNames = descriptor.split(`,`);
      const nameSet = new Set(optNames);
      return advanced_options_utils.makeCommandOption({
        definition(builder) {
          builder.addOption({
            names: optNames,
            arity,
            hidden: opts === null || opts === void 0 ? void 0 : opts.hidden,
            description: opts === null || opts === void 0 ? void 0 : opts.description,
            required: opts.required
          });
        },
        transformer(builder, key, state) {
          let usedName;
          let currentValue = typeof initialValue !== `undefined` ? [...initialValue] : void 0;
          for (const { name: name2, value } of state.options) {
            if (!nameSet.has(name2))
              continue;
            usedName = name2;
            currentValue = currentValue !== null && currentValue !== void 0 ? currentValue : [];
            currentValue.push(value);
          }
          if (typeof currentValue !== `undefined`) {
            return advanced_options_utils.applyValidator(usedName !== null && usedName !== void 0 ? usedName : key, currentValue, opts.validator);
          } else {
            return currentValue;
          }
        }
      });
    }
    exports2.Array = Array2;
  }
});

// node_modules/clipanion/lib/advanced/options/Boolean.js
var require_Boolean = __commonJS({
  "node_modules/clipanion/lib/advanced/options/Boolean.js"(exports2) {
    "use strict";
    Object.defineProperty(exports2, "__esModule", { value: true });
    var advanced_options_utils = require_utils();
    function Boolean2(descriptor, initialValueBase, optsBase) {
      const [initialValue, opts] = advanced_options_utils.rerouteArguments(initialValueBase, optsBase !== null && optsBase !== void 0 ? optsBase : {});
      const optNames = descriptor.split(`,`);
      const nameSet = new Set(optNames);
      return advanced_options_utils.makeCommandOption({
        definition(builder) {
          builder.addOption({
            names: optNames,
            allowBinding: false,
            arity: 0,
            hidden: opts.hidden,
            description: opts.description,
            required: opts.required
          });
        },
        transformer(builer, key, state) {
          let currentValue = initialValue;
          for (const { name: name2, value } of state.options) {
            if (!nameSet.has(name2))
              continue;
            currentValue = value;
          }
          return currentValue;
        }
      });
    }
    exports2.Boolean = Boolean2;
  }
});

// node_modules/clipanion/lib/advanced/options/Counter.js
var require_Counter = __commonJS({
  "node_modules/clipanion/lib/advanced/options/Counter.js"(exports2) {
    "use strict";
    Object.defineProperty(exports2, "__esModule", { value: true });
    var advanced_options_utils = require_utils();
    function Counter(descriptor, initialValueBase, optsBase) {
      const [initialValue, opts] = advanced_options_utils.rerouteArguments(initialValueBase, optsBase !== null && optsBase !== void 0 ? optsBase : {});
      const optNames = descriptor.split(`,`);
      const nameSet = new Set(optNames);
      return advanced_options_utils.makeCommandOption({
        definition(builder) {
          builder.addOption({
            names: optNames,
            allowBinding: false,
            arity: 0,
            hidden: opts.hidden,
            description: opts.description,
            required: opts.required
          });
        },
        transformer(builder, key, state) {
          let currentValue = initialValue;
          for (const { name: name2, value } of state.options) {
            if (!nameSet.has(name2))
              continue;
            currentValue !== null && currentValue !== void 0 ? currentValue : currentValue = 0;
            if (!value) {
              currentValue = 0;
            } else {
              currentValue += 1;
            }
          }
          return currentValue;
        }
      });
    }
    exports2.Counter = Counter;
  }
});

// node_modules/clipanion/lib/advanced/options/Proxy.js
var require_Proxy = __commonJS({
  "node_modules/clipanion/lib/advanced/options/Proxy.js"(exports2) {
    "use strict";
    Object.defineProperty(exports2, "__esModule", { value: true });
    var advanced_options_utils = require_utils();
    function Proxy2(opts = {}) {
      return advanced_options_utils.makeCommandOption({
        definition(builder, key) {
          var _a;
          builder.addProxy({
            name: (_a = opts.name) !== null && _a !== void 0 ? _a : key,
            required: opts.required
          });
        },
        transformer(builder, key, state) {
          return state.positionals.map(({ value }) => value);
        }
      });
    }
    exports2.Proxy = Proxy2;
  }
});

// node_modules/clipanion/lib/advanced/options/Rest.js
var require_Rest = __commonJS({
  "node_modules/clipanion/lib/advanced/options/Rest.js"(exports2) {
    "use strict";
    Object.defineProperty(exports2, "__esModule", { value: true });
    var core = require_core();
    var advanced_options_utils = require_utils();
    function Rest(opts = {}) {
      return advanced_options_utils.makeCommandOption({
        definition(builder, key) {
          var _a;
          builder.addRest({
            name: (_a = opts.name) !== null && _a !== void 0 ? _a : key,
            required: opts.required
          });
        },
        transformer(builder, key, state) {
          const isRestPositional = (index) => {
            const positional = state.positionals[index];
            if (positional.extra === core.NoLimits)
              return true;
            if (positional.extra === false && index < builder.arity.leading.length)
              return true;
            return false;
          };
          let count = 0;
          while (count < state.positionals.length && isRestPositional(count))
            count += 1;
          return state.positionals.splice(0, count).map(({ value }) => value);
        }
      });
    }
    exports2.Rest = Rest;
  }
});

// node_modules/clipanion/lib/advanced/options/String.js
var require_String = __commonJS({
  "node_modules/clipanion/lib/advanced/options/String.js"(exports2) {
    "use strict";
    Object.defineProperty(exports2, "__esModule", { value: true });
    var core = require_core();
    var advanced_options_utils = require_utils();
    function StringOption(descriptor, initialValueBase, optsBase) {
      const [initialValue, opts] = advanced_options_utils.rerouteArguments(initialValueBase, optsBase !== null && optsBase !== void 0 ? optsBase : {});
      const { arity = 1 } = opts;
      const optNames = descriptor.split(`,`);
      const nameSet = new Set(optNames);
      return advanced_options_utils.makeCommandOption({
        definition(builder) {
          builder.addOption({
            names: optNames,
            arity: opts.tolerateBoolean ? 0 : arity,
            hidden: opts.hidden,
            description: opts.description,
            required: opts.required
          });
        },
        transformer(builder, key, state, context) {
          let usedName;
          let currentValue = initialValue;
          if (typeof opts.env !== `undefined` && context.env[opts.env]) {
            usedName = opts.env;
            currentValue = context.env[opts.env];
          }
          for (const { name: name2, value } of state.options) {
            if (!nameSet.has(name2))
              continue;
            usedName = name2;
            currentValue = value;
          }
          if (typeof currentValue === `string`) {
            return advanced_options_utils.applyValidator(usedName !== null && usedName !== void 0 ? usedName : key, currentValue, opts.validator);
          } else {
            return currentValue;
          }
        }
      });
    }
    function StringPositional(opts = {}) {
      const { required = true } = opts;
      return advanced_options_utils.makeCommandOption({
        definition(builder, key) {
          var _a;
          builder.addPositional({
            name: (_a = opts.name) !== null && _a !== void 0 ? _a : key,
            required: opts.required
          });
        },
        transformer(builder, key, state) {
          var _a;
          for (let i = 0; i < state.positionals.length; ++i) {
            if (state.positionals[i].extra === core.NoLimits)
              continue;
            if (required && state.positionals[i].extra === true)
              continue;
            if (!required && state.positionals[i].extra === false)
              continue;
            const [positional] = state.positionals.splice(i, 1);
            return advanced_options_utils.applyValidator((_a = opts.name) !== null && _a !== void 0 ? _a : key, positional.value, opts.validator);
          }
          return void 0;
        }
      });
    }
    function String2(descriptor, ...args) {
      if (typeof descriptor === `string`) {
        return StringOption(descriptor, ...args);
      } else {
        return StringPositional(descriptor);
      }
    }
    exports2.String = String2;
  }
});

// node_modules/clipanion/lib/advanced/options/index.js
var require_options = __commonJS({
  "node_modules/clipanion/lib/advanced/options/index.js"(exports2) {
    "use strict";
    Object.defineProperty(exports2, "__esModule", { value: true });
    var advanced_options_utils = require_utils();
    var advanced_options_Array = require_Array();
    var advanced_options_Boolean = require_Boolean();
    var advanced_options_Counter = require_Counter();
    var advanced_options_Proxy = require_Proxy();
    var advanced_options_Rest = require_Rest();
    var advanced_options_String = require_String();
    exports2.applyValidator = advanced_options_utils.applyValidator;
    exports2.cleanValidationError = advanced_options_utils.cleanValidationError;
    exports2.formatError = advanced_options_utils.formatError;
    exports2.isOptionSymbol = advanced_options_utils.isOptionSymbol;
    exports2.makeCommandOption = advanced_options_utils.makeCommandOption;
    exports2.rerouteArguments = advanced_options_utils.rerouteArguments;
    exports2.Array = advanced_options_Array.Array;
    exports2.Boolean = advanced_options_Boolean.Boolean;
    exports2.Counter = advanced_options_Counter.Counter;
    exports2.Proxy = advanced_options_Proxy.Proxy;
    exports2.Rest = advanced_options_Rest.Rest;
    exports2.String = advanced_options_String.String;
  }
});

// node_modules/clipanion/lib/advanced/index.js
var require_advanced = __commonJS({
  "node_modules/clipanion/lib/advanced/index.js"(exports2) {
    "use strict";
    Object.defineProperty(exports2, "__esModule", { value: true });
    var errors = require_errors();
    var format = require_format();
    var advanced_Command = require_Command();
    var advanced_Cli = require_Cli();
    var advanced_builtins_index = require_builtins();
    var advanced_options_index = require_options();
    exports2.UsageError = errors.UsageError;
    exports2.formatMarkdownish = format.formatMarkdownish;
    exports2.Command = advanced_Command.Command;
    exports2.Cli = advanced_Cli.Cli;
    exports2.run = advanced_Cli.run;
    exports2.runExit = advanced_Cli.runExit;
    exports2.Builtins = advanced_builtins_index;
    exports2.Option = advanced_options_index;
  }
});

// node_modules/semver/internal/debug.js
var require_debug = __commonJS({
  "node_modules/semver/internal/debug.js"(exports2, module2) {
    "use strict";
    var debug = typeof process === "object" && process.env && process.env.NODE_DEBUG && /\bsemver\b/i.test(process.env.NODE_DEBUG) ? (...args) => console.error("SEMVER", ...args) : () => {
    };
    module2.exports = debug;
  }
});

// node_modules/semver/internal/constants.js
var require_constants2 = __commonJS({
  "node_modules/semver/internal/constants.js"(exports2, module2) {
    "use strict";
    var SEMVER_SPEC_VERSION = "2.0.0";
    var MAX_LENGTH = 256;
    var MAX_SAFE_INTEGER = Number.MAX_SAFE_INTEGER || /* istanbul ignore next */
    9007199254740991;
    var MAX_SAFE_COMPONENT_LENGTH = 16;
    var MAX_SAFE_BUILD_LENGTH = MAX_LENGTH - 6;
    var RELEASE_TYPES = [
      "major",
      "premajor",
      "minor",
      "preminor",
      "patch",
      "prepatch",
      "prerelease"
    ];
    module2.exports = {
      MAX_LENGTH,
      MAX_SAFE_COMPONENT_LENGTH,
      MAX_SAFE_BUILD_LENGTH,
      MAX_SAFE_INTEGER,
      RELEASE_TYPES,
      SEMVER_SPEC_VERSION,
      FLAG_INCLUDE_PRERELEASE: 1,
      FLAG_LOOSE: 2
    };
  }
});

// node_modules/semver/internal/re.js
var require_re = __commonJS({
  "node_modules/semver/internal/re.js"(exports2, module2) {
    "use strict";
    var {
      MAX_SAFE_COMPONENT_LENGTH,
      MAX_SAFE_BUILD_LENGTH,
      MAX_LENGTH
    } = require_constants2();
    var debug = require_debug();
    exports2 = module2.exports = {};
    var re = exports2.re = [];
    var safeRe = exports2.safeRe = [];
    var src = exports2.src = [];
    var safeSrc = exports2.safeSrc = [];
    var t = exports2.t = {};
    var R = 0;
    var LETTERDASHNUMBER = "[a-zA-Z0-9-]";
    var safeRegexReplacements = [
      ["\\s", 1],
      ["\\d", MAX_LENGTH],
      [LETTERDASHNUMBER, MAX_SAFE_BUILD_LENGTH]
    ];
    var makeSafeRegex = (value) => {
      for (const [token, max] of safeRegexReplacements) {
        value = value.split(`${token}*`).join(`${token}{0,${max}}`).split(`${token}+`).join(`${token}{1,${max}}`);
      }
      return value;
    };
    var createToken = (name2, value, isGlobal) => {
      const safe = makeSafeRegex(value);
      const index = R++;
      debug(name2, index, value);
      t[name2] = index;
      src[index] = value;
      safeSrc[index] = safe;
      re[index] = new RegExp(value, isGlobal ? "g" : void 0);
      safeRe[index] = new RegExp(safe, isGlobal ? "g" : void 0);
    };
    createToken("NUMERICIDENTIFIER", "0|[1-9]\\d*");
    createToken("NUMERICIDENTIFIERLOOSE", "\\d+");
    createToken("NONNUMERICIDENTIFIER", `\\d*[a-zA-Z-]${LETTERDASHNUMBER}*`);
    createToken("MAINVERSION", `(${src[t.NUMERICIDENTIFIER]})\\.(${src[t.NUMERICIDENTIFIER]})\\.(${src[t.NUMERICIDENTIFIER]})`);
    createToken("MAINVERSIONLOOSE", `(${src[t.NUMERICIDENTIFIERLOOSE]})\\.(${src[t.NUMERICIDENTIFIERLOOSE]})\\.(${src[t.NUMERICIDENTIFIERLOOSE]})`);
    createToken("PRERELEASEIDENTIFIER", `(?:${src[t.NONNUMERICIDENTIFIER]}|${src[t.NUMERICIDENTIFIER]})`);
    createToken("PRERELEASEIDENTIFIERLOOSE", `(?:${src[t.NONNUMERICIDENTIFIER]}|${src[t.NUMERICIDENTIFIERLOOSE]})`);
    createToken("PRERELEASE", `(?:-(${src[t.PRERELEASEIDENTIFIER]}(?:\\.${src[t.PRERELEASEIDENTIFIER]})*))`);
    createToken("PRERELEASELOOSE", `(?:-?(${src[t.PRERELEASEIDENTIFIERLOOSE]}(?:\\.${src[t.PRERELEASEIDENTIFIERLOOSE]})*))`);
    createToken("BUILDIDENTIFIER", `${LETTERDASHNUMBER}+`);
    createToken("BUILD", `(?:\\+(${src[t.BUILDIDENTIFIER]}(?:\\.${src[t.BUILDIDENTIFIER]})*))`);
    createToken("FULLPLAIN", `v?${src[t.MAINVERSION]}${src[t.PRERELEASE]}?${src[t.BUILD]}?`);
    createToken("FULL", `^${src[t.FULLPLAIN]}$`);
    createToken("LOOSEPLAIN", `[v=\\s]*${src[t.MAINVERSIONLOOSE]}${src[t.PRERELEASELOOSE]}?${src[t.BUILD]}?`);
    createToken("LOOSE", `^${src[t.LOOSEPLAIN]}$`);
    createToken("GTLT", "((?:<|>)?=?)");
    createToken("XRANGEIDENTIFIERLOOSE", `${src[t.NUMERICIDENTIFIERLOOSE]}|x|X|\\*`);
    createToken("XRANGEIDENTIFIER", `${src[t.NUMERICIDENTIFIER]}|x|X|\\*`);
    createToken("XRANGEPLAIN", `[v=\\s]*(${src[t.XRANGEIDENTIFIER]})(?:\\.(${src[t.XRANGEIDENTIFIER]})(?:\\.(${src[t.XRANGEIDENTIFIER]})(?:${src[t.PRERELEASE]})?${src[t.BUILD]}?)?)?`);
    createToken("XRANGEPLAINLOOSE", `[v=\\s]*(${src[t.XRANGEIDENTIFIERLOOSE]})(?:\\.(${src[t.XRANGEIDENTIFIERLOOSE]})(?:\\.(${src[t.XRANGEIDENTIFIERLOOSE]})(?:${src[t.PRERELEASELOOSE]})?${src[t.BUILD]}?)?)?`);
    createToken("XRANGE", `^${src[t.GTLT]}\\s*${src[t.XRANGEPLAIN]}$`);
    createToken("XRANGELOOSE", `^${src[t.GTLT]}\\s*${src[t.XRANGEPLAINLOOSE]}$`);
    createToken("COERCEPLAIN", `${"(^|[^\\d])(\\d{1,"}${MAX_SAFE_COMPONENT_LENGTH}})(?:\\.(\\d{1,${MAX_SAFE_COMPONENT_LENGTH}}))?(?:\\.(\\d{1,${MAX_SAFE_COMPONENT_LENGTH}}))?`);
    createToken("COERCE", `${src[t.COERCEPLAIN]}(?:$|[^\\d])`);
    createToken("COERCEFULL", src[t.COERCEPLAIN] + `(?:${src[t.PRERELEASE]})?(?:${src[t.BUILD]})?(?:$|[^\\d])`);
    createToken("COERCERTL", src[t.COERCE], true);
    createToken("COERCERTLFULL", src[t.COERCEFULL], true);
    createToken("LONETILDE", "(?:~>?)");
    createToken("TILDETRIM", `(\\s*)${src[t.LONETILDE]}\\s+`, true);
    exports2.tildeTrimReplace = "$1~";
    createToken("TILDE", `^${src[t.LONETILDE]}${src[t.XRANGEPLAIN]}$`);
    createToken("TILDELOOSE", `^${src[t.LONETILDE]}${src[t.XRANGEPLAINLOOSE]}$`);
    createToken("LONECARET", "(?:\\^)");
    createToken("CARETTRIM", `(\\s*)${src[t.LONECARET]}\\s+`, true);
    exports2.caretTrimReplace = "$1^";
    createToken("CARET", `^${src[t.LONECARET]}${src[t.XRANGEPLAIN]}$`);
    createToken("CARETLOOSE", `^${src[t.LONECARET]}${src[t.XRANGEPLAINLOOSE]}$`);
    createToken("COMPARATORLOOSE", `^${src[t.GTLT]}\\s*(${src[t.LOOSEPLAIN]})$|^$`);
    createToken("COMPARATOR", `^${src[t.GTLT]}\\s*(${src[t.FULLPLAIN]})$|^$`);
    createToken("COMPARATORTRIM", `(\\s*)${src[t.GTLT]}\\s*(${src[t.LOOSEPLAIN]}|${src[t.XRANGEPLAIN]})`, true);
    exports2.comparatorTrimReplace = "$1$2$3";
    createToken("HYPHENRANGE", `^\\s*(${src[t.XRANGEPLAIN]})\\s+-\\s+(${src[t.XRANGEPLAIN]})\\s*$`);
    createToken("HYPHENRANGELOOSE", `^\\s*(${src[t.XRANGEPLAINLOOSE]})\\s+-\\s+(${src[t.XRANGEPLAINLOOSE]})\\s*$`);
    createToken("STAR", "(<|>)?=?\\s*\\*");
    createToken("GTE0", "^\\s*>=\\s*0\\.0\\.0\\s*$");
    createToken("GTE0PRE", "^\\s*>=\\s*0\\.0\\.0-0\\s*$");
  }
});

// node_modules/semver/internal/parse-options.js
var require_parse_options = __commonJS({
  "node_modules/semver/internal/parse-options.js"(exports2, module2) {
    "use strict";
    var looseOption = Object.freeze({ loose: true });
    var emptyOpts = Object.freeze({});
    var parseOptions = (options) => {
      if (!options) {
        return emptyOpts;
      }
      if (typeof options !== "object") {
        return looseOption;
      }
      return options;
    };
    module2.exports = parseOptions;
  }
});

// node_modules/semver/internal/identifiers.js
var require_identifiers = __commonJS({
  "node_modules/semver/internal/identifiers.js"(exports2, module2) {
    "use strict";
    var numeric = /^[0-9]+$/;
    var compareIdentifiers = (a, b) => {
      if (typeof a === "number" && typeof b === "number") {
        return a === b ? 0 : a < b ? -1 : 1;
      }
      const anum = numeric.test(a);
      const bnum = numeric.test(b);
      if (anum && bnum) {
        a = +a;
        b = +b;
      }
      return a === b ? 0 : anum && !bnum ? -1 : bnum && !anum ? 1 : a < b ? -1 : 1;
    };
    var rcompareIdentifiers = (a, b) => compareIdentifiers(b, a);
    module2.exports = {
      compareIdentifiers,
      rcompareIdentifiers
    };
  }
});

// node_modules/semver/classes/semver.js
var require_semver = __commonJS({
  "node_modules/semver/classes/semver.js"(exports2, module2) {
    "use strict";
    var debug = require_debug();
    var { MAX_LENGTH, MAX_SAFE_INTEGER } = require_constants2();
    var { safeRe: re, t } = require_re();
    var parseOptions = require_parse_options();
    var { compareIdentifiers } = require_identifiers();
    var SemVer3 = class _SemVer {
      constructor(version2, options) {
        options = parseOptions(options);
        if (version2 instanceof _SemVer) {
          if (version2.loose === !!options.loose && version2.includePrerelease === !!options.includePrerelease) {
            return version2;
          } else {
            version2 = version2.version;
          }
        } else if (typeof version2 !== "string") {
          throw new TypeError(`Invalid version. Must be a string. Got type "${typeof version2}".`);
        }
        if (version2.length > MAX_LENGTH) {
          throw new TypeError(
            `version is longer than ${MAX_LENGTH} characters`
          );
        }
        debug("SemVer", version2, options);
        this.options = options;
        this.loose = !!options.loose;
        this.includePrerelease = !!options.includePrerelease;
        const m = version2.trim().match(options.loose ? re[t.LOOSE] : re[t.FULL]);
        if (!m) {
          throw new TypeError(`Invalid Version: ${version2}`);
        }
        this.raw = version2;
        this.major = +m[1];
        this.minor = +m[2];
        this.patch = +m[3];
        if (this.major > MAX_SAFE_INTEGER || this.major < 0) {
          throw new TypeError("Invalid major version");
        }
        if (this.minor > MAX_SAFE_INTEGER || this.minor < 0) {
          throw new TypeError("Invalid minor version");
        }
        if (this.patch > MAX_SAFE_INTEGER || this.patch < 0) {
          throw new TypeError("Invalid patch version");
        }
        if (!m[4]) {
          this.prerelease = [];
        } else {
          this.prerelease = m[4].split(".").map((id) => {
            if (/^[0-9]+$/.test(id)) {
              const num = +id;
              if (num >= 0 && num < MAX_SAFE_INTEGER) {
                return num;
              }
            }
            return id;
          });
        }
        this.build = m[5] ? m[5].split(".") : [];
        this.format();
      }
      format() {
        this.version = `${this.major}.${this.minor}.${this.patch}`;
        if (this.prerelease.length) {
          this.version += `-${this.prerelease.join(".")}`;
        }
        return this.version;
      }
      toString() {
        return this.version;
      }
      compare(other) {
        debug("SemVer.compare", this.version, this.options, other);
        if (!(other instanceof _SemVer)) {
          if (typeof other === "string" && other === this.version) {
            return 0;
          }
          other = new _SemVer(other, this.options);
        }
        if (other.version === this.version) {
          return 0;
        }
        return this.compareMain(other) || this.comparePre(other);
      }
      compareMain(other) {
        if (!(other instanceof _SemVer)) {
          other = new _SemVer(other, this.options);
        }
        if (this.major < other.major) {
          return -1;
        }
        if (this.major > other.major) {
          return 1;
        }
        if (this.minor < other.minor) {
          return -1;
        }
        if (this.minor > other.minor) {
          return 1;
        }
        if (this.patch < other.patch) {
          return -1;
        }
        if (this.patch > other.patch) {
          return 1;
        }
        return 0;
      }
      comparePre(other) {
        if (!(other instanceof _SemVer)) {
          other = new _SemVer(other, this.options);
        }
        if (this.prerelease.length && !other.prerelease.length) {
          return -1;
        } else if (!this.prerelease.length && other.prerelease.length) {
          return 1;
        } else if (!this.prerelease.length && !other.prerelease.length) {
          return 0;
        }
        let i = 0;
        do {
          const a = this.prerelease[i];
          const b = other.prerelease[i];
          debug("prerelease compare", i, a, b);
          if (a === void 0 && b === void 0) {
            return 0;
          } else if (b === void 0) {
            return 1;
          } else if (a === void 0) {
            return -1;
          } else if (a === b) {
            continue;
          } else {
            return compareIdentifiers(a, b);
          }
        } while (++i);
      }
      compareBuild(other) {
        if (!(other instanceof _SemVer)) {
          other = new _SemVer(other, this.options);
        }
        let i = 0;
        do {
          const a = this.build[i];
          const b = other.build[i];
          debug("build compare", i, a, b);
          if (a === void 0 && b === void 0) {
            return 0;
          } else if (b === void 0) {
            return 1;
          } else if (a === void 0) {
            return -1;
          } else if (a === b) {
            continue;
          } else {
            return compareIdentifiers(a, b);
          }
        } while (++i);
      }
      // preminor will bump the version up to the next minor release, and immediately
      // down to pre-release. premajor and prepatch work the same way.
      inc(release, identifier, identifierBase) {
        if (release.startsWith("pre")) {
          if (!identifier && identifierBase === false) {
            throw new Error("invalid increment argument: identifier is empty");
          }
          if (identifier) {
            const match = `-${identifier}`.match(this.options.loose ? re[t.PRERELEASELOOSE] : re[t.PRERELEASE]);
            if (!match || match[1] !== identifier) {
              throw new Error(`invalid identifier: ${identifier}`);
            }
          }
        }
        switch (release) {
          case "premajor":
            this.prerelease.length = 0;
            this.patch = 0;
            this.minor = 0;
            this.major++;
            this.inc("pre", identifier, identifierBase);
            break;
          case "preminor":
            this.prerelease.length = 0;
            this.patch = 0;
            this.minor++;
            this.inc("pre", identifier, identifierBase);
            break;
          case "prepatch":
            this.prerelease.length = 0;
            this.inc("patch", identifier, identifierBase);
            this.inc("pre", identifier, identifierBase);
            break;
          // If the input is a non-prerelease version, this acts the same as
          // prepatch.
          case "prerelease":
            if (this.prerelease.length === 0) {
              this.inc("patch", identifier, identifierBase);
            }
            this.inc("pre", identifier, identifierBase);
            break;
          case "release":
            if (this.prerelease.length === 0) {
              throw new Error(`version ${this.raw} is not a prerelease`);
            }
            this.prerelease.length = 0;
            break;
          case "major":
            if (this.minor !== 0 || this.patch !== 0 || this.prerelease.length === 0) {
              this.major++;
            }
            this.minor = 0;
            this.patch = 0;
            this.prerelease = [];
            break;
          case "minor":
            if (this.patch !== 0 || this.prerelease.length === 0) {
              this.minor++;
            }
            this.patch = 0;
            this.prerelease = [];
            break;
          case "patch":
            if (this.prerelease.length === 0) {
              this.patch++;
            }
            this.prerelease = [];
            break;
          // This probably shouldn't be used publicly.
          // 1.0.0 'pre' would become 1.0.0-0 which is the wrong direction.
          case "pre": {
            const base = Number(identifierBase) ? 1 : 0;
            if (this.prerelease.length === 0) {
              this.prerelease = [base];
            } else {
              let i = this.prerelease.length;
              while (--i >= 0) {
                if (typeof this.prerelease[i] === "number") {
                  this.prerelease[i]++;
                  i = -2;
                }
              }
              if (i === -1) {
                if (identifier === this.prerelease.join(".") && identifierBase === false) {
                  throw new Error("invalid increment argument: identifier already exists");
                }
                this.prerelease.push(base);
              }
            }
            if (identifier) {
              let prerelease = [identifier, base];
              if (identifierBase === false) {
                prerelease = [identifier];
              }
              if (compareIdentifiers(this.prerelease[0], identifier) === 0) {
                if (isNaN(this.prerelease[1])) {
                  this.prerelease = prerelease;
                }
              } else {
                this.prerelease = prerelease;
              }
            }
            break;
          }
          default:
            throw new Error(`invalid increment argument: ${release}`);
        }
        this.raw = this.format();
        if (this.build.length) {
          this.raw += `+${this.build.join(".")}`;
        }
        return this;
      }
    };
    module2.exports = SemVer3;
  }
});

// node_modules/semver/functions/compare.js
var require_compare = __commonJS({
  "node_modules/semver/functions/compare.js"(exports2, module2) {
    "use strict";
    var SemVer3 = require_semver();
    var compare = (a, b, loose) => new SemVer3(a, loose).compare(new SemVer3(b, loose));
    module2.exports = compare;
  }
});

// node_modules/semver/functions/rcompare.js
var require_rcompare = __commonJS({
  "node_modules/semver/functions/rcompare.js"(exports2, module2) {
    "use strict";
    var compare = require_compare();
    var rcompare = (a, b, loose) => compare(b, a, loose);
    module2.exports = rcompare;
  }
});

// node_modules/semver/functions/parse.js
var require_parse = __commonJS({
  "node_modules/semver/functions/parse.js"(exports2, module2) {
    "use strict";
    var SemVer3 = require_semver();
    var parse4 = (version2, options, throwErrors = false) => {
      if (version2 instanceof SemVer3) {
        return version2;
      }
      try {
        return new SemVer3(version2, options);
      } catch (er) {
        if (!throwErrors) {
          return null;
        }
        throw er;
      }
    };
    module2.exports = parse4;
  }
});

// node_modules/semver/functions/valid.js
var require_valid = __commonJS({
  "node_modules/semver/functions/valid.js"(exports2, module2) {
    "use strict";
    var parse4 = require_parse();
    var valid = (version2, options) => {
      const v = parse4(version2, options);
      return v ? v.version : null;
    };
    module2.exports = valid;
  }
});

// node_modules/semver/internal/lrucache.js
var require_lrucache = __commonJS({
  "node_modules/semver/internal/lrucache.js"(exports2, module2) {
    "use strict";
    var LRUCache = class {
      constructor() {
        this.max = 1e3;
        this.map = /* @__PURE__ */ new Map();
      }
      get(key) {
        const value = this.map.get(key);
        if (value === void 0) {
          return void 0;
        } else {
          this.map.delete(key);
          this.map.set(key, value);
          return value;
        }
      }
      delete(key) {
        return this.map.delete(key);
      }
      set(key, value) {
        const deleted = this.delete(key);
        if (!deleted && value !== void 0) {
          if (this.map.size >= this.max) {
            const firstKey = this.map.keys().next().value;
            this.delete(firstKey);
          }
          this.map.set(key, value);
        }
        return this;
      }
    };
    module2.exports = LRUCache;
  }
});

// node_modules/semver/functions/eq.js
var require_eq = __commonJS({
  "node_modules/semver/functions/eq.js"(exports2, module2) {
    "use strict";
    var compare = require_compare();
    var eq = (a, b, loose) => compare(a, b, loose) === 0;
    module2.exports = eq;
  }
});

// node_modules/semver/functions/neq.js
var require_neq = __commonJS({
  "node_modules/semver/functions/neq.js"(exports2, module2) {
    "use strict";
    var compare = require_compare();
    var neq = (a, b, loose) => compare(a, b, loose) !== 0;
    module2.exports = neq;
  }
});

// node_modules/semver/functions/gt.js
var require_gt = __commonJS({
  "node_modules/semver/functions/gt.js"(exports2, module2) {
    "use strict";
    var compare = require_compare();
    var gt = (a, b, loose) => compare(a, b, loose) > 0;
    module2.exports = gt;
  }
});

// node_modules/semver/functions/gte.js
var require_gte = __commonJS({
  "node_modules/semver/functions/gte.js"(exports2, module2) {
    "use strict";
    var compare = require_compare();
    var gte = (a, b, loose) => compare(a, b, loose) >= 0;
    module2.exports = gte;
  }
});

// node_modules/semver/functions/lt.js
var require_lt = __commonJS({
  "node_modules/semver/functions/lt.js"(exports2, module2) {
    "use strict";
    var compare = require_compare();
    var lt = (a, b, loose) => compare(a, b, loose) < 0;
    module2.exports = lt;
  }
});

// node_modules/semver/functions/lte.js
var require_lte = __commonJS({
  "node_modules/semver/functions/lte.js"(exports2, module2) {
    "use strict";
    var compare = require_compare();
    var lte = (a, b, loose) => compare(a, b, loose) <= 0;
    module2.exports = lte;
  }
});

// node_modules/semver/functions/cmp.js
var require_cmp = __commonJS({
  "node_modules/semver/functions/cmp.js"(exports2, module2) {
    "use strict";
    var eq = require_eq();
    var neq = require_neq();
    var gt = require_gt();
    var gte = require_gte();
    var lt = require_lt();
    var lte = require_lte();
    var cmp = (a, op, b, loose) => {
      switch (op) {
        case "===":
          if (typeof a === "object") {
            a = a.version;
          }
          if (typeof b === "object") {
            b = b.version;
          }
          return a === b;
        case "!==":
          if (typeof a === "object") {
            a = a.version;
          }
          if (typeof b === "object") {
            b = b.version;
          }
          return a !== b;
        case "":
        case "=":
        case "==":
          return eq(a, b, loose);
        case "!=":
          return neq(a, b, loose);
        case ">":
          return gt(a, b, loose);
        case ">=":
          return gte(a, b, loose);
        case "<":
          return lt(a, b, loose);
        case "<=":
          return lte(a, b, loose);
        default:
          throw new TypeError(`Invalid operator: ${op}`);
      }
    };
    module2.exports = cmp;
  }
});

// node_modules/semver/classes/comparator.js
var require_comparator = __commonJS({
  "node_modules/semver/classes/comparator.js"(exports2, module2) {
    "use strict";
    var ANY = /* @__PURE__ */ Symbol("SemVer ANY");
    var Comparator = class _Comparator {
      static get ANY() {
        return ANY;
      }
      constructor(comp, options) {
        options = parseOptions(options);
        if (comp instanceof _Comparator) {
          if (comp.loose === !!options.loose) {
            return comp;
          } else {
            comp = comp.value;
          }
        }
        comp = comp.trim().split(/\s+/).join(" ");
        debug("comparator", comp, options);
        this.options = options;
        this.loose = !!options.loose;
        this.parse(comp);
        if (this.semver === ANY) {
          this.value = "";
        } else {
          this.value = this.operator + this.semver.version;
        }
        debug("comp", this);
      }
      parse(comp) {
        const r = this.options.loose ? re[t.COMPARATORLOOSE] : re[t.COMPARATOR];
        const m = comp.match(r);
        if (!m) {
          throw new TypeError(`Invalid comparator: ${comp}`);
        }
        this.operator = m[1] !== void 0 ? m[1] : "";
        if (this.operator === "=") {
          this.operator = "";
        }
        if (!m[2]) {
          this.semver = ANY;
        } else {
          this.semver = new SemVer3(m[2], this.options.loose);
        }
      }
      toString() {
        return this.value;
      }
      test(version2) {
        debug("Comparator.test", version2, this.options.loose);
        if (this.semver === ANY || version2 === ANY) {
          return true;
        }
        if (typeof version2 === "string") {
          try {
            version2 = new SemVer3(version2, this.options);
          } catch (er) {
            return false;
          }
        }
        return cmp(version2, this.operator, this.semver, this.options);
      }
      intersects(comp, options) {
        if (!(comp instanceof _Comparator)) {
          throw new TypeError("a Comparator is required");
        }
        if (this.operator === "") {
          if (this.value === "") {
            return true;
          }
          return new Range3(comp.value, options).test(this.value);
        } else if (comp.operator === "") {
          if (comp.value === "") {
            return true;
          }
          return new Range3(this.value, options).test(comp.semver);
        }
        options = parseOptions(options);
        if (options.includePrerelease && (this.value === "<0.0.0-0" || comp.value === "<0.0.0-0")) {
          return false;
        }
        if (!options.includePrerelease && (this.value.startsWith("<0.0.0") || comp.value.startsWith("<0.0.0"))) {
          return false;
        }
        if (this.operator.startsWith(">") && comp.operator.startsWith(">")) {
          return true;
        }
        if (this.operator.startsWith("<") && comp.operator.startsWith("<")) {
          return true;
        }
        if (this.semver.version === comp.semver.version && this.operator.includes("=") && comp.operator.includes("=")) {
          return true;
        }
        if (cmp(this.semver, "<", comp.semver, options) && this.operator.startsWith(">") && comp.operator.startsWith("<")) {
          return true;
        }
        if (cmp(this.semver, ">", comp.semver, options) && this.operator.startsWith("<") && comp.operator.startsWith(">")) {
          return true;
        }
        return false;
      }
    };
    module2.exports = Comparator;
    var parseOptions = require_parse_options();
    var { safeRe: re, t } = require_re();
    var cmp = require_cmp();
    var debug = require_debug();
    var SemVer3 = require_semver();
    var Range3 = require_range();
  }
});

// node_modules/semver/classes/range.js
var require_range = __commonJS({
  "node_modules/semver/classes/range.js"(exports2, module2) {
    "use strict";
    var SPACE_CHARACTERS = /\s+/g;
    var Range3 = class _Range {
      constructor(range, options) {
        options = parseOptions(options);
        if (range instanceof _Range) {
          if (range.loose === !!options.loose && range.includePrerelease === !!options.includePrerelease) {
            return range;
          } else {
            return new _Range(range.raw, options);
          }
        }
        if (range instanceof Comparator) {
          this.raw = range.value;
          this.set = [[range]];
          this.formatted = void 0;
          return this;
        }
        this.options = options;
        this.loose = !!options.loose;
        this.includePrerelease = !!options.includePrerelease;
        this.raw = range.trim().replace(SPACE_CHARACTERS, " ");
        this.set = this.raw.split("||").map((r) => this.parseRange(r.trim())).filter((c) => c.length);
        if (!this.set.length) {
          throw new TypeError(`Invalid SemVer Range: ${this.raw}`);
        }
        if (this.set.length > 1) {
          const first = this.set[0];
          this.set = this.set.filter((c) => !isNullSet(c[0]));
          if (this.set.length === 0) {
            this.set = [first];
          } else if (this.set.length > 1) {
            for (const c of this.set) {
              if (c.length === 1 && isAny(c[0])) {
                this.set = [c];
                break;
              }
            }
          }
        }
        this.formatted = void 0;
      }
      get range() {
        if (this.formatted === void 0) {
          this.formatted = "";
          for (let i = 0; i < this.set.length; i++) {
            if (i > 0) {
              this.formatted += "||";
            }
            const comps = this.set[i];
            for (let k = 0; k < comps.length; k++) {
              if (k > 0) {
                this.formatted += " ";
              }
              this.formatted += comps[k].toString().trim();
            }
          }
        }
        return this.formatted;
      }
      format() {
        return this.range;
      }
      toString() {
        return this.range;
      }
      parseRange(range) {
        const memoOpts = (this.options.includePrerelease && FLAG_INCLUDE_PRERELEASE) | (this.options.loose && FLAG_LOOSE);
        const memoKey = memoOpts + ":" + range;
        const cached = cache2.get(memoKey);
        if (cached) {
          return cached;
        }
        const loose = this.options.loose;
        const hr = loose ? re[t.HYPHENRANGELOOSE] : re[t.HYPHENRANGE];
        range = range.replace(hr, hyphenReplace(this.options.includePrerelease));
        debug("hyphen replace", range);
        range = range.replace(re[t.COMPARATORTRIM], comparatorTrimReplace);
        debug("comparator trim", range);
        range = range.replace(re[t.TILDETRIM], tildeTrimReplace);
        debug("tilde trim", range);
        range = range.replace(re[t.CARETTRIM], caretTrimReplace);
        debug("caret trim", range);
        let rangeList = range.split(" ").map((comp) => parseComparator(comp, this.options)).join(" ").split(/\s+/).map((comp) => replaceGTE0(comp, this.options));
        if (loose) {
          rangeList = rangeList.filter((comp) => {
            debug("loose invalid filter", comp, this.options);
            return !!comp.match(re[t.COMPARATORLOOSE]);
          });
        }
        debug("range list", rangeList);
        const rangeMap = /* @__PURE__ */ new Map();
        const comparators = rangeList.map((comp) => new Comparator(comp, this.options));
        for (const comp of comparators) {
          if (isNullSet(comp)) {
            return [comp];
          }
          rangeMap.set(comp.value, comp);
        }
        if (rangeMap.size > 1 && rangeMap.has("")) {
          rangeMap.delete("");
        }
        const result = [...rangeMap.values()];
        cache2.set(memoKey, result);
        return result;
      }
      intersects(range, options) {
        if (!(range instanceof _Range)) {
          throw new TypeError("a Range is required");
        }
        return this.set.some((thisComparators) => {
          return isSatisfiable(thisComparators, options) && range.set.some((rangeComparators) => {
            return isSatisfiable(rangeComparators, options) && thisComparators.every((thisComparator) => {
              return rangeComparators.every((rangeComparator) => {
                return thisComparator.intersects(rangeComparator, options);
              });
            });
          });
        });
      }
      // if ANY of the sets match ALL of its comparators, then pass
      test(version2) {
        if (!version2) {
          return false;
        }
        if (typeof version2 === "string") {
          try {
            version2 = new SemVer3(version2, this.options);
          } catch (er) {
            return false;
          }
        }
        for (let i = 0; i < this.set.length; i++) {
          if (testSet(this.set[i], version2, this.options)) {
            return true;
          }
        }
        return false;
      }
    };
    module2.exports = Range3;
    var LRU = require_lrucache();
    var cache2 = new LRU();
    var parseOptions = require_parse_options();
    var Comparator = require_comparator();
    var debug = require_debug();
    var SemVer3 = require_semver();
    var {
      safeRe: re,
      t,
      comparatorTrimReplace,
      tildeTrimReplace,
      caretTrimReplace
    } = require_re();
    var { FLAG_INCLUDE_PRERELEASE, FLAG_LOOSE } = require_constants2();
    var isNullSet = (c) => c.value === "<0.0.0-0";
    var isAny = (c) => c.value === "";
    var isSatisfiable = (comparators, options) => {
      let result = true;
      const remainingComparators = comparators.slice();
      let testComparator = remainingComparators.pop();
      while (result && remainingComparators.length) {
        result = remainingComparators.every((otherComparator) => {
          return testComparator.intersects(otherComparator, options);
        });
        testComparator = remainingComparators.pop();
      }
      return result;
    };
    var parseComparator = (comp, options) => {
      comp = comp.replace(re[t.BUILD], "");
      debug("comp", comp, options);
      comp = replaceCarets(comp, options);
      debug("caret", comp);
      comp = replaceTildes(comp, options);
      debug("tildes", comp);
      comp = replaceXRanges(comp, options);
      debug("xrange", comp);
      comp = replaceStars(comp, options);
      debug("stars", comp);
      return comp;
    };
    var isX = (id) => !id || id.toLowerCase() === "x" || id === "*";
    var replaceTildes = (comp, options) => {
      return comp.trim().split(/\s+/).map((c) => replaceTilde(c, options)).join(" ");
    };
    var replaceTilde = (comp, options) => {
      const r = options.loose ? re[t.TILDELOOSE] : re[t.TILDE];
      return comp.replace(r, (_, M, m, p, pr) => {
        debug("tilde", comp, _, M, m, p, pr);
        let ret;
        if (isX(M)) {
          ret = "";
        } else if (isX(m)) {
          ret = `>=${M}.0.0 <${+M + 1}.0.0-0`;
        } else if (isX(p)) {
          ret = `>=${M}.${m}.0 <${M}.${+m + 1}.0-0`;
        } else if (pr) {
          debug("replaceTilde pr", pr);
          ret = `>=${M}.${m}.${p}-${pr} <${M}.${+m + 1}.0-0`;
        } else {
          ret = `>=${M}.${m}.${p} <${M}.${+m + 1}.0-0`;
        }
        debug("tilde return", ret);
        return ret;
      });
    };
    var replaceCarets = (comp, options) => {
      return comp.trim().split(/\s+/).map((c) => replaceCaret(c, options)).join(" ");
    };
    var replaceCaret = (comp, options) => {
      debug("caret", comp, options);
      const r = options.loose ? re[t.CARETLOOSE] : re[t.CARET];
      const z = options.includePrerelease ? "-0" : "";
      return comp.replace(r, (_, M, m, p, pr) => {
        debug("caret", comp, _, M, m, p, pr);
        let ret;
        if (isX(M)) {
          ret = "";
        } else if (isX(m)) {
          ret = `>=${M}.0.0${z} <${+M + 1}.0.0-0`;
        } else if (isX(p)) {
          if (M === "0") {
            ret = `>=${M}.${m}.0${z} <${M}.${+m + 1}.0-0`;
          } else {
            ret = `>=${M}.${m}.0${z} <${+M + 1}.0.0-0`;
          }
        } else if (pr) {
          debug("replaceCaret pr", pr);
          if (M === "0") {
            if (m === "0") {
              ret = `>=${M}.${m}.${p}-${pr} <${M}.${m}.${+p + 1}-0`;
            } else {
              ret = `>=${M}.${m}.${p}-${pr} <${M}.${+m + 1}.0-0`;
            }
          } else {
            ret = `>=${M}.${m}.${p}-${pr} <${+M + 1}.0.0-0`;
          }
        } else {
          debug("no pr");
          if (M === "0") {
            if (m === "0") {
              ret = `>=${M}.${m}.${p}${z} <${M}.${m}.${+p + 1}-0`;
            } else {
              ret = `>=${M}.${m}.${p}${z} <${M}.${+m + 1}.0-0`;
            }
          } else {
            ret = `>=${M}.${m}.${p} <${+M + 1}.0.0-0`;
          }
        }
        debug("caret return", ret);
        return ret;
      });
    };
    var replaceXRanges = (comp, options) => {
      debug("replaceXRanges", comp, options);
      return comp.split(/\s+/).map((c) => replaceXRange(c, options)).join(" ");
    };
    var replaceXRange = (comp, options) => {
      comp = comp.trim();
      const r = options.loose ? re[t.XRANGELOOSE] : re[t.XRANGE];
      return comp.replace(r, (ret, gtlt, M, m, p, pr) => {
        debug("xRange", comp, ret, gtlt, M, m, p, pr);
        const xM = isX(M);
        const xm = xM || isX(m);
        const xp = xm || isX(p);
        const anyX = xp;
        if (gtlt === "=" && anyX) {
          gtlt = "";
        }
        pr = options.includePrerelease ? "-0" : "";
        if (xM) {
          if (gtlt === ">" || gtlt === "<") {
            ret = "<0.0.0-0";
          } else {
            ret = "*";
          }
        } else if (gtlt && anyX) {
          if (xm) {
            m = 0;
          }
          p = 0;
          if (gtlt === ">") {
            gtlt = ">=";
            if (xm) {
              M = +M + 1;
              m = 0;
              p = 0;
            } else {
              m = +m + 1;
              p = 0;
            }
          } else if (gtlt === "<=") {
            gtlt = "<";
            if (xm) {
              M = +M + 1;
            } else {
              m = +m + 1;
            }
          }
          if (gtlt === "<") {
            pr = "-0";
          }
          ret = `${gtlt + M}.${m}.${p}${pr}`;
        } else if (xm) {
          ret = `>=${M}.0.0${pr} <${+M + 1}.0.0-0`;
        } else if (xp) {
          ret = `>=${M}.${m}.0${pr} <${M}.${+m + 1}.0-0`;
        }
        debug("xRange return", ret);
        return ret;
      });
    };
    var replaceStars = (comp, options) => {
      debug("replaceStars", comp, options);
      return comp.trim().replace(re[t.STAR], "");
    };
    var replaceGTE0 = (comp, options) => {
      debug("replaceGTE0", comp, options);
      return comp.trim().replace(re[options.includePrerelease ? t.GTE0PRE : t.GTE0], "");
    };
    var hyphenReplace = (incPr) => ($0, from, fM, fm, fp, fpr, fb, to, tM, tm, tp, tpr) => {
      if (isX(fM)) {
        from = "";
      } else if (isX(fm)) {
        from = `>=${fM}.0.0${incPr ? "-0" : ""}`;
      } else if (isX(fp)) {
        from = `>=${fM}.${fm}.0${incPr ? "-0" : ""}`;
      } else if (fpr) {
        from = `>=${from}`;
      } else {
        from = `>=${from}${incPr ? "-0" : ""}`;
      }
      if (isX(tM)) {
        to = "";
      } else if (isX(tm)) {
        to = `<${+tM + 1}.0.0-0`;
      } else if (isX(tp)) {
        to = `<${tM}.${+tm + 1}.0-0`;
      } else if (tpr) {
        to = `<=${tM}.${tm}.${tp}-${tpr}`;
      } else if (incPr) {
        to = `<${tM}.${tm}.${+tp + 1}-0`;
      } else {
        to = `<=${to}`;
      }
      return `${from} ${to}`.trim();
    };
    var testSet = (set, version2, options) => {
      for (let i = 0; i < set.length; i++) {
        if (!set[i].test(version2)) {
          return false;
        }
      }
      if (version2.prerelease.length && !options.includePrerelease) {
        for (let i = 0; i < set.length; i++) {
          debug(set[i].semver);
          if (set[i].semver === Comparator.ANY) {
            continue;
          }
          if (set[i].semver.prerelease.length > 0) {
            const allowed = set[i].semver;
            if (allowed.major === version2.major && allowed.minor === version2.minor && allowed.patch === version2.patch) {
              return true;
            }
          }
        }
        return false;
      }
      return true;
    };
  }
});

// node_modules/semver/ranges/valid.js
var require_valid2 = __commonJS({
  "node_modules/semver/ranges/valid.js"(exports2, module2) {
    "use strict";
    var Range3 = require_range();
    var validRange = (range, options) => {
      try {
        return new Range3(range, options).range || "*";
      } catch (er) {
        return null;
      }
    };
    module2.exports = validRange;
  }
});

// node_modules/ms/index.js
var require_ms = __commonJS({
  "node_modules/ms/index.js"(exports2, module2) {
    var s = 1e3;
    var m = s * 60;
    var h = m * 60;
    var d = h * 24;
    var w = d * 7;
    var y = d * 365.25;
    module2.exports = function(val, options) {
      options = options || {};
      var type = typeof val;
      if (type === "string" && val.length > 0) {
        return parse4(val);
      } else if (type === "number" && isFinite(val)) {
        return options.long ? fmtLong(val) : fmtShort(val);
      }
      throw new Error(
        "val is not a non-empty string or a valid number. val=" + JSON.stringify(val)
      );
    };
    function parse4(str) {
      str = String(str);
      if (str.length > 100) {
        return;
      }
      var match = /^(-?(?:\d+)?\.?\d+) *(milliseconds?|msecs?|ms|seconds?|secs?|s|minutes?|mins?|m|hours?|hrs?|h|days?|d|weeks?|w|years?|yrs?|y)?$/i.exec(
        str
      );
      if (!match) {
        return;
      }
      var n = parseFloat(match[1]);
      var type = (match[2] || "ms").toLowerCase();
      switch (type) {
        case "years":
        case "year":
        case "yrs":
        case "yr":
        case "y":
          return n * y;
        case "weeks":
        case "week":
        case "w":
          return n * w;
        case "days":
        case "day":
        case "d":
          return n * d;
        case "hours":
        case "hour":
        case "hrs":
        case "hr":
        case "h":
          return n * h;
        case "minutes":
        case "minute":
        case "mins":
        case "min":
        case "m":
          return n * m;
        case "seconds":
        case "second":
        case "secs":
        case "sec":
        case "s":
          return n * s;
        case "milliseconds":
        case "millisecond":
        case "msecs":
        case "msec":
        case "ms":
          return n;
        default:
          return void 0;
      }
    }
    function fmtShort(ms) {
      var msAbs = Math.abs(ms);
      if (msAbs >= d) {
        return Math.round(ms / d) + "d";
      }
      if (msAbs >= h) {
        return Math.round(ms / h) + "h";
      }
      if (msAbs >= m) {
        return Math.round(ms / m) + "m";
      }
      if (msAbs >= s) {
        return Math.round(ms / s) + "s";
      }
      return ms + "ms";
    }
    function fmtLong(ms) {
      var msAbs = Math.abs(ms);
      if (msAbs >= d) {
        return plural2(ms, msAbs, d, "day");
      }
      if (msAbs >= h) {
        return plural2(ms, msAbs, h, "hour");
      }
      if (msAbs >= m) {
        return plural2(ms, msAbs, m, "minute");
      }
      if (msAbs >= s) {
        return plural2(ms, msAbs, s, "second");
      }
      return ms + " ms";
    }
    function plural2(ms, msAbs, n, name2) {
      var isPlural = msAbs >= n * 1.5;
      return Math.round(ms / n) + " " + name2 + (isPlural ? "s" : "");
    }
  }
});

// node_modules/debug/src/common.js
var require_common = __commonJS({
  "node_modules/debug/src/common.js"(exports2, module2) {
    function setup(env2) {
      createDebug.debug = createDebug;
      createDebug.default = createDebug;
      createDebug.coerce = coerce;
      createDebug.disable = disable;
      createDebug.enable = enable;
      createDebug.enabled = enabled;
      createDebug.humanize = require_ms();
      createDebug.destroy = destroy;
      Object.keys(env2).forEach((key) => {
        createDebug[key] = env2[key];
      });
      createDebug.names = [];
      createDebug.skips = [];
      createDebug.formatters = {};
      function selectColor(namespace) {
        let hash = 0;
        for (let i = 0; i < namespace.length; i++) {
          hash = (hash << 5) - hash + namespace.charCodeAt(i);
          hash |= 0;
        }
        return createDebug.colors[Math.abs(hash) % createDebug.colors.length];
      }
      createDebug.selectColor = selectColor;
      function createDebug(namespace) {
        let prevTime;
        let enableOverride = null;
        let namespacesCache;
        let enabledCache;
        function debug(...args) {
          if (!debug.enabled) {
            return;
          }
          const self2 = debug;
          const curr = Number(/* @__PURE__ */ new Date());
          const ms = curr - (prevTime || curr);
          self2.diff = ms;
          self2.prev = prevTime;
          self2.curr = curr;
          prevTime = curr;
          args[0] = createDebug.coerce(args[0]);
          if (typeof args[0] !== "string") {
            args.unshift("%O");
          }
          let index = 0;
          args[0] = args[0].replace(/%([a-zA-Z%])/g, (match, format) => {
            if (match === "%%") {
              return "%";
            }
            index++;
            const formatter = createDebug.formatters[format];
            if (typeof formatter === "function") {
              const val = args[index];
              match = formatter.call(self2, val);
              args.splice(index, 1);
              index--;
            }
            return match;
          });
          createDebug.formatArgs.call(self2, args);
          const logFn = self2.log || createDebug.log;
          logFn.apply(self2, args);
        }
        debug.namespace = namespace;
        debug.useColors = createDebug.useColors();
        debug.color = createDebug.selectColor(namespace);
        debug.extend = extend;
        debug.destroy = createDebug.destroy;
        Object.defineProperty(debug, "enabled", {
          enumerable: true,
          configurable: false,
          get: () => {
            if (enableOverride !== null) {
              return enableOverride;
            }
            if (namespacesCache !== createDebug.namespaces) {
              namespacesCache = createDebug.namespaces;
              enabledCache = createDebug.enabled(namespace);
            }
            return enabledCache;
          },
          set: (v) => {
            enableOverride = v;
          }
        });
        if (typeof createDebug.init === "function") {
          createDebug.init(debug);
        }
        return debug;
      }
      function extend(namespace, delimiter) {
        const newDebug = createDebug(this.namespace + (typeof delimiter === "undefined" ? ":" : delimiter) + namespace);
        newDebug.log = this.log;
        return newDebug;
      }
      function enable(namespaces) {
        createDebug.save(namespaces);
        createDebug.namespaces = namespaces;
        createDebug.names = [];
        createDebug.skips = [];
        const split = (typeof namespaces === "string" ? namespaces : "").trim().replace(/\s+/g, ",").split(",").filter(Boolean);
        for (const ns of split) {
          if (ns[0] === "-") {
            createDebug.skips.push(ns.slice(1));
          } else {
            createDebug.names.push(ns);
          }
        }
      }
      function matchesTemplate(search, template) {
        let searchIndex = 0;
        let templateIndex = 0;
        let starIndex = -1;
        let matchIndex = 0;
        while (searchIndex < search.length) {
          if (templateIndex < template.length && (template[templateIndex] === search[searchIndex] || template[templateIndex] === "*")) {
            if (template[templateIndex] === "*") {
              starIndex = templateIndex;
              matchIndex = searchIndex;
              templateIndex++;
            } else {
              searchIndex++;
              templateIndex++;
            }
          } else if (starIndex !== -1) {
            templateIndex = starIndex + 1;
            matchIndex++;
            searchIndex = matchIndex;
          } else {
            return false;
          }
        }
        while (templateIndex < template.length && template[templateIndex] === "*") {
          templateIndex++;
        }
        return templateIndex === template.length;
      }
      function disable() {
        const namespaces = [
          ...createDebug.names,
          ...createDebug.skips.map((namespace) => "-" + namespace)
        ].join(",");
        createDebug.enable("");
        return namespaces;
      }
      function enabled(name2) {
        for (const skip of createDebug.skips) {
          if (matchesTemplate(name2, skip)) {
            return false;
          }
        }
        for (const ns of createDebug.names) {
          if (matchesTemplate(name2, ns)) {
            return true;
          }
        }
        return false;
      }
      function coerce(val) {
        if (val instanceof Error) {
          return val.stack || val.message;
        }
        return val;
      }
      function destroy() {
        console.warn("Instance method `debug.destroy()` is deprecated and no longer does anything. It will be removed in the next major version of `debug`.");
      }
      createDebug.enable(createDebug.load());
      return createDebug;
    }
    module2.exports = setup;
  }
});

// node_modules/debug/src/browser.js
var require_browser = __commonJS({
  "node_modules/debug/src/browser.js"(exports2, module2) {
    exports2.formatArgs = formatArgs;
    exports2.save = save;
    exports2.load = load;
    exports2.useColors = useColors;
    exports2.storage = localstorage();
    exports2.destroy = /* @__PURE__ */ (() => {
      let warned = false;
      return () => {
        if (!warned) {
          warned = true;
          console.warn("Instance method `debug.destroy()` is deprecated and no longer does anything. It will be removed in the next major version of `debug`.");
        }
      };
    })();
    exports2.colors = [
      "#0000CC",
      "#0000FF",
      "#0033CC",
      "#0033FF",
      "#0066CC",
      "#0066FF",
      "#0099CC",
      "#0099FF",
      "#00CC00",
      "#00CC33",
      "#00CC66",
      "#00CC99",
      "#00CCCC",
      "#00CCFF",
      "#3300CC",
      "#3300FF",
      "#3333CC",
      "#3333FF",
      "#3366CC",
      "#3366FF",
      "#3399CC",
      "#3399FF",
      "#33CC00",
      "#33CC33",
      "#33CC66",
      "#33CC99",
      "#33CCCC",
      "#33CCFF",
      "#6600CC",
      "#6600FF",
      "#6633CC",
      "#6633FF",
      "#66CC00",
      "#66CC33",
      "#9900CC",
      "#9900FF",
      "#9933CC",
      "#9933FF",
      "#99CC00",
      "#99CC33",
      "#CC0000",
      "#CC0033",
      "#CC0066",
      "#CC0099",
      "#CC00CC",
      "#CC00FF",
      "#CC3300",
      "#CC3333",
      "#CC3366",
      "#CC3399",
      "#CC33CC",
      "#CC33FF",
      "#CC6600",
      "#CC6633",
      "#CC9900",
      "#CC9933",
      "#CCCC00",
      "#CCCC33",
      "#FF0000",
      "#FF0033",
      "#FF0066",
      "#FF0099",
      "#FF00CC",
      "#FF00FF",
      "#FF3300",
      "#FF3333",
      "#FF3366",
      "#FF3399",
      "#FF33CC",
      "#FF33FF",
      "#FF6600",
      "#FF6633",
      "#FF9900",
      "#FF9933",
      "#FFCC00",
      "#FFCC33"
    ];
    function useColors() {
      if (typeof window !== "undefined" && window.process && (window.process.type === "renderer" || window.process.__nwjs)) {
        return true;
      }
      if (typeof navigator !== "undefined" && navigator.userAgent && navigator.userAgent.toLowerCase().match(/(edge|trident)\/(\d+)/)) {
        return false;
      }
      let m;
      return typeof document !== "undefined" && document.documentElement && document.documentElement.style && document.documentElement.style.WebkitAppearance || // Is firebug? http://stackoverflow.com/a/398120/376773
      typeof window !== "undefined" && window.console && (window.console.firebug || window.console.exception && window.console.table) || // Is firefox >= v31?
      // https://developer.mozilla.org/en-US/docs/Tools/Web_Console#Styling_messages
      typeof navigator !== "undefined" && navigator.userAgent && (m = navigator.userAgent.toLowerCase().match(/firefox\/(\d+)/)) && parseInt(m[1], 10) >= 31 || // Double check webkit in userAgent just in case we are in a worker
      typeof navigator !== "undefined" && navigator.userAgent && navigator.userAgent.toLowerCase().match(/applewebkit\/(\d+)/);
    }
    function formatArgs(args) {
      args[0] = (this.useColors ? "%c" : "") + this.namespace + (this.useColors ? " %c" : " ") + args[0] + (this.useColors ? "%c " : " ") + "+" + module2.exports.humanize(this.diff);
      if (!this.useColors) {
        return;
      }
      const c = "color: " + this.color;
      args.splice(1, 0, c, "color: inherit");
      let index = 0;
      let lastC = 0;
      args[0].replace(/%[a-zA-Z%]/g, (match) => {
        if (match === "%%") {
          return;
        }
        index++;
        if (match === "%c") {
          lastC = index;
        }
      });
      args.splice(lastC, 0, c);
    }
    exports2.log = console.debug || console.log || (() => {
    });
    function save(namespaces) {
      try {
        if (namespaces) {
          exports2.storage.setItem("debug", namespaces);
        } else {
          exports2.storage.removeItem("debug");
        }
      } catch (error) {
      }
    }
    function load() {
      let r;
      try {
        r = exports2.storage.getItem("debug") || exports2.storage.getItem("DEBUG");
      } catch (error) {
      }
      if (!r && typeof process !== "undefined" && "env" in process) {
        r = process.env.DEBUG;
      }
      return r;
    }
    function localstorage() {
      try {
        return localStorage;
      } catch (error) {
      }
    }
    module2.exports = require_common()(exports2);
    var { formatters } = module2.exports;
    formatters.j = function(v) {
      try {
        return JSON.stringify(v);
      } catch (error) {
        return "[UnexpectedJSONParseError]: " + error.message;
      }
    };
  }
});

// node_modules/supports-color/index.js
var supports_color_exports = {};
__export(supports_color_exports, {
  createSupportsColor: () => createSupportsColor,
  default: () => supports_color_default
});
function hasFlag(flag, argv = globalThis.Deno ? globalThis.Deno.args : import_node_process.default.argv) {
  const prefix = flag.startsWith("-") ? "" : flag.length === 1 ? "-" : "--";
  const position = argv.indexOf(prefix + flag);
  const terminatorPosition = argv.indexOf("--");
  return position !== -1 && (terminatorPosition === -1 || position < terminatorPosition);
}
function envForceColor() {
  if (!("FORCE_COLOR" in env)) {
    return;
  }
  if (env.FORCE_COLOR === "true") {
    return 1;
  }
  if (env.FORCE_COLOR === "false") {
    return 0;
  }
  if (env.FORCE_COLOR.length === 0) {
    return 1;
  }
  const level = Math.min(Number.parseInt(env.FORCE_COLOR, 10), 3);
  if (![0, 1, 2, 3].includes(level)) {
    return;
  }
  return level;
}
function translateLevel(level) {
  if (level === 0) {
    return false;
  }
  return {
    level,
    hasBasic: true,
    has256: level >= 2,
    has16m: level >= 3
  };
}
function _supportsColor(haveStream, { streamIsTTY, sniffFlags = true } = {}) {
  const noFlagForceColor = envForceColor();
  if (noFlagForceColor !== void 0) {
    flagForceColor = noFlagForceColor;
  }
  const forceColor = sniffFlags ? flagForceColor : noFlagForceColor;
  if (forceColor === 0) {
    return 0;
  }
  if (sniffFlags) {
    if (hasFlag("color=16m") || hasFlag("color=full") || hasFlag("color=truecolor")) {
      return 3;
    }
    if (hasFlag("color=256")) {
      return 2;
    }
  }
  if ("TF_BUILD" in env && "AGENT_NAME" in env) {
    return 1;
  }
  if (haveStream && !streamIsTTY && forceColor === void 0) {
    return 0;
  }
  const min = forceColor || 0;
  if (env.TERM === "dumb") {
    return min;
  }
  if (import_node_process.default.platform === "win32") {
    const osRelease = import_node_os.default.release().split(".");
    if (Number(osRelease[0]) >= 10 && Number(osRelease[2]) >= 10586) {
      return Number(osRelease[2]) >= 14931 ? 3 : 2;
    }
    return 1;
  }
  if ("CI" in env) {
    if (["GITHUB_ACTIONS", "GITEA_ACTIONS", "CIRCLECI"].some((key) => key in env)) {
      return 3;
    }
    if (["TRAVIS", "APPVEYOR", "GITLAB_CI", "BUILDKITE", "DRONE"].some((sign) => sign in env) || env.CI_NAME === "codeship") {
      return 1;
    }
    return min;
  }
  if ("TEAMCITY_VERSION" in env) {
    return /^(9\.(0*[1-9]\d*)\.|\d{2,}\.)/.test(env.TEAMCITY_VERSION) ? 1 : 0;
  }
  if (env.COLORTERM === "truecolor") {
    return 3;
  }
  if (env.TERM === "xterm-kitty") {
    return 3;
  }
  if (env.TERM === "xterm-ghostty") {
    return 3;
  }
  if (env.TERM === "wezterm") {
    return 3;
  }
  if ("TERM_PROGRAM" in env) {
    const version2 = Number.parseInt((env.TERM_PROGRAM_VERSION || "").split(".")[0], 10);
    switch (env.TERM_PROGRAM) {
      case "iTerm.app": {
        return version2 >= 3 ? 3 : 2;
      }
      case "Apple_Terminal": {
        return 2;
      }
    }
  }
  if (/-256(color)?$/i.test(env.TERM)) {
    return 2;
  }
  if (/^screen|^xterm|^vt100|^vt220|^rxvt|color|ansi|cygwin|linux/i.test(env.TERM)) {
    return 1;
  }
  if ("COLORTERM" in env) {
    return 1;
  }
  return min;
}
function createSupportsColor(stream, options = {}) {
  const level = _supportsColor(stream, {
    streamIsTTY: stream && stream.isTTY,
    ...options
  });
  return translateLevel(level);
}
var import_node_process, import_node_os, import_node_tty, env, flagForceColor, supportsColor, supports_color_default;
var init_supports_color = __esm({
  "node_modules/supports-color/index.js"() {
    import_node_process = __toESM(require("node:process"), 1);
    import_node_os = __toESM(require("node:os"), 1);
    import_node_tty = __toESM(require("node:tty"), 1);
    ({ env } = import_node_process.default);
    if (hasFlag("no-color") || hasFlag("no-colors") || hasFlag("color=false") || hasFlag("color=never")) {
      flagForceColor = 0;
    } else if (hasFlag("color") || hasFlag("colors") || hasFlag("color=true") || hasFlag("color=always")) {
      flagForceColor = 1;
    }
    supportsColor = {
      stdout: createSupportsColor({ isTTY: import_node_tty.default.isatty(1) }),
      stderr: createSupportsColor({ isTTY: import_node_tty.default.isatty(2) })
    };
    supports_color_default = supportsColor;
  }
});

// node_modules/debug/src/node.js
var require_node = __commonJS({
  "node_modules/debug/src/node.js"(exports2, module2) {
    var tty3 = require("tty");
    var util = require("util");
    exports2.init = init;
    exports2.log = log2;
    exports2.formatArgs = formatArgs;
    exports2.save = save;
    exports2.load = load;
    exports2.useColors = useColors;
    exports2.destroy = util.deprecate(
      () => {
      },
      "Instance method `debug.destroy()` is deprecated and no longer does anything. It will be removed in the next major version of `debug`."
    );
    exports2.colors = [6, 2, 3, 4, 5, 1];
    try {
      const supportsColor2 = (init_supports_color(), __toCommonJS(supports_color_exports));
      if (supportsColor2 && (supportsColor2.stderr || supportsColor2).level >= 2) {
        exports2.colors = [
          20,
          21,
          26,
          27,
          32,
          33,
          38,
          39,
          40,
          41,
          42,
          43,
          44,
          45,
          56,
          57,
          62,
          63,
          68,
          69,
          74,
          75,
          76,
          77,
          78,
          79,
          80,
          81,
          92,
          93,
          98,
          99,
          112,
          113,
          128,
          129,
          134,
          135,
          148,
          149,
          160,
          161,
          162,
          163,
          164,
          165,
          166,
          167,
          168,
          169,
          170,
          171,
          172,
          173,
          178,
          179,
          184,
          185,
          196,
          197,
          198,
          199,
          200,
          201,
          202,
          203,
          204,
          205,
          206,
          207,
          208,
          209,
          214,
          215,
          220,
          221
        ];
      }
    } catch (error) {
    }
    exports2.inspectOpts = Object.keys(process.env).filter((key) => {
      return /^debug_/i.test(key);
    }).reduce((obj, key) => {
      const prop = key.substring(6).toLowerCase().replace(/_([a-z])/g, (_, k) => {
        return k.toUpperCase();
      });
      let val = process.env[key];
      if (/^(yes|on|true|enabled)$/i.test(val)) {
        val = true;
      } else if (/^(no|off|false|disabled)$/i.test(val)) {
        val = false;
      } else if (val === "null") {
        val = null;
      } else {
        val = Number(val);
      }
      obj[prop] = val;
      return obj;
    }, {});
    function useColors() {
      return "colors" in exports2.inspectOpts ? Boolean(exports2.inspectOpts.colors) : tty3.isatty(process.stderr.fd);
    }
    function formatArgs(args) {
      const { namespace: name2, useColors: useColors2 } = this;
      if (useColors2) {
        const c = this.color;
        const colorCode = "\x1B[3" + (c < 8 ? c : "8;5;" + c);
        const prefix = `  ${colorCode};1m${name2} \x1B[0m`;
        args[0] = prefix + args[0].split("\n").join("\n" + prefix);
        args.push(colorCode + "m+" + module2.exports.humanize(this.diff) + "\x1B[0m");
      } else {
        args[0] = getDate() + name2 + " " + args[0];
      }
    }
    function getDate() {
      if (exports2.inspectOpts.hideDate) {
        return "";
      }
      return (/* @__PURE__ */ new Date()).toISOString() + " ";
    }
    function log2(...args) {
      return process.stderr.write(util.formatWithOptions(exports2.inspectOpts, ...args) + "\n");
    }
    function save(namespaces) {
      if (namespaces) {
        process.env.DEBUG = namespaces;
      } else {
        delete process.env.DEBUG;
      }
    }
    function load() {
      return process.env.DEBUG;
    }
    function init(debug) {
      debug.inspectOpts = {};
      const keys = Object.keys(exports2.inspectOpts);
      for (let i = 0; i < keys.length; i++) {
        debug.inspectOpts[keys[i]] = exports2.inspectOpts[keys[i]];
      }
    }
    module2.exports = require_common()(exports2);
    var { formatters } = module2.exports;
    formatters.o = function(v) {
      this.inspectOpts.colors = this.useColors;
      return util.inspect(v, this.inspectOpts).split("\n").map((str) => str.trim()).join(" ");
    };
    formatters.O = function(v) {
      this.inspectOpts.colors = this.useColors;
      return util.inspect(v, this.inspectOpts);
    };
  }
});

// node_modules/debug/src/index.js
var require_src = __commonJS({
  "node_modules/debug/src/index.js"(exports2, module2) {
    if (typeof process === "undefined" || process.type === "renderer" || process.browser === true || process.__nwjs) {
      module2.exports = require_browser();
    } else {
      module2.exports = require_node();
    }
  }
});

// node_modules/minipass/dist/esm/index.js
var import_node_events, import_node_stream, import_node_string_decoder, proc, isStream, isReadable, isWritable, EOF, MAYBE_EMIT_END, EMITTED_END, EMITTING_END, EMITTED_ERROR, CLOSED, READ, FLUSH, FLUSHCHUNK, ENCODING, DECODER, FLOWING, PAUSED, RESUME, BUFFER, PIPES, BUFFERLENGTH, BUFFERPUSH, BUFFERSHIFT, OBJECTMODE, DESTROYED, ERROR, EMITDATA, EMITEND, EMITEND2, ASYNC, ABORT, ABORTED, SIGNAL, DATALISTENERS, DISCARDED, defer, nodefer, isEndish, isArrayBufferLike, isArrayBufferView, Pipe, PipeProxyErrors, isObjectModeOptions, isEncodingOptions, Minipass;
var init_esm = __esm({
  "node_modules/minipass/dist/esm/index.js"() {
    import_node_events = require("node:events");
    import_node_stream = __toESM(require("node:stream"), 1);
    import_node_string_decoder = require("node:string_decoder");
    proc = typeof process === "object" && process ? process : {
      stdout: null,
      stderr: null
    };
    isStream = (s) => !!s && typeof s === "object" && (s instanceof Minipass || s instanceof import_node_stream.default || isReadable(s) || isWritable(s));
    isReadable = (s) => !!s && typeof s === "object" && s instanceof import_node_events.EventEmitter && typeof s.pipe === "function" && // node core Writable streams have a pipe() method, but it throws
    s.pipe !== import_node_stream.default.Writable.prototype.pipe;
    isWritable = (s) => !!s && typeof s === "object" && s instanceof import_node_events.EventEmitter && typeof s.write === "function" && typeof s.end === "function";
    EOF = /* @__PURE__ */ Symbol("EOF");
    MAYBE_EMIT_END = /* @__PURE__ */ Symbol("maybeEmitEnd");
    EMITTED_END = /* @__PURE__ */ Symbol("emittedEnd");
    EMITTING_END = /* @__PURE__ */ Symbol("emittingEnd");
    EMITTED_ERROR = /* @__PURE__ */ Symbol("emittedError");
    CLOSED = /* @__PURE__ */ Symbol("closed");
    READ = /* @__PURE__ */ Symbol("read");
    FLUSH = /* @__PURE__ */ Symbol("flush");
    FLUSHCHUNK = /* @__PURE__ */ Symbol("flushChunk");
    ENCODING = /* @__PURE__ */ Symbol("encoding");
    DECODER = /* @__PURE__ */ Symbol("decoder");
    FLOWING = /* @__PURE__ */ Symbol("flowing");
    PAUSED = /* @__PURE__ */ Symbol("paused");
    RESUME = /* @__PURE__ */ Symbol("resume");
    BUFFER = /* @__PURE__ */ Symbol("buffer");
    PIPES = /* @__PURE__ */ Symbol("pipes");
    BUFFERLENGTH = /* @__PURE__ */ Symbol("bufferLength");
    BUFFERPUSH = /* @__PURE__ */ Symbol("bufferPush");
    BUFFERSHIFT = /* @__PURE__ */ Symbol("bufferShift");
    OBJECTMODE = /* @__PURE__ */ Symbol("objectMode");
    DESTROYED = /* @__PURE__ */ Symbol("destroyed");
    ERROR = /* @__PURE__ */ Symbol("error");
    EMITDATA = /* @__PURE__ */ Symbol("emitData");
    EMITEND = /* @__PURE__ */ Symbol("emitEnd");
    EMITEND2 = /* @__PURE__ */ Symbol("emitEnd2");
    ASYNC = /* @__PURE__ */ Symbol("async");
    ABORT = /* @__PURE__ */ Symbol("abort");
    ABORTED = /* @__PURE__ */ Symbol("aborted");
    SIGNAL = /* @__PURE__ */ Symbol("signal");
    DATALISTENERS = /* @__PURE__ */ Symbol("dataListeners");
    DISCARDED = /* @__PURE__ */ Symbol("discarded");
    defer = (fn2) => Promise.resolve().then(fn2);
    nodefer = (fn2) => fn2();
    isEndish = (ev) => ev === "end" || ev === "finish" || ev === "prefinish";
    isArrayBufferLike = (b) => b instanceof ArrayBuffer || !!b && typeof b === "object" && b.constructor && b.constructor.name === "ArrayBuffer" && b.byteLength >= 0;
    isArrayBufferView = (b) => !Buffer.isBuffer(b) && ArrayBuffer.isView(b);
    Pipe = class {
      src;
      dest;
      opts;
      ondrain;
      constructor(src, dest, opts) {
        this.src = src;
        this.dest = dest;
        this.opts = opts;
        this.ondrain = () => src[RESUME]();
        this.dest.on("drain", this.ondrain);
      }
      unpipe() {
        this.dest.removeListener("drain", this.ondrain);
      }
      // only here for the prototype
      /* c8 ignore start */
      proxyErrors(_er) {
      }
      /* c8 ignore stop */
      end() {
        this.unpipe();
        if (this.opts.end)
          this.dest.end();
      }
    };
    PipeProxyErrors = class extends Pipe {
      unpipe() {
        this.src.removeListener("error", this.proxyErrors);
        super.unpipe();
      }
      constructor(src, dest, opts) {
        super(src, dest, opts);
        this.proxyErrors = (er) => this.dest.emit("error", er);
        src.on("error", this.proxyErrors);
      }
    };
    isObjectModeOptions = (o) => !!o.objectMode;
    isEncodingOptions = (o) => !o.objectMode && !!o.encoding && o.encoding !== "buffer";
    Minipass = class extends import_node_events.EventEmitter {
      [FLOWING] = false;
      [PAUSED] = false;
      [PIPES] = [];
      [BUFFER] = [];
      [OBJECTMODE];
      [ENCODING];
      [ASYNC];
      [DECODER];
      [EOF] = false;
      [EMITTED_END] = false;
      [EMITTING_END] = false;
      [CLOSED] = false;
      [EMITTED_ERROR] = null;
      [BUFFERLENGTH] = 0;
      [DESTROYED] = false;
      [SIGNAL];
      [ABORTED] = false;
      [DATALISTENERS] = 0;
      [DISCARDED] = false;
      /**
       * true if the stream can be written
       */
      writable = true;
      /**
       * true if the stream can be read
       */
      readable = true;
      /**
       * If `RType` is Buffer, then options do not need to be provided.
       * Otherwise, an options object must be provided to specify either
       * {@link Minipass.SharedOptions.objectMode} or
       * {@link Minipass.SharedOptions.encoding}, as appropriate.
       */
      constructor(...args) {
        const options = args[0] || {};
        super();
        if (options.objectMode && typeof options.encoding === "string") {
          throw new TypeError("Encoding and objectMode may not be used together");
        }
        if (isObjectModeOptions(options)) {
          this[OBJECTMODE] = true;
          this[ENCODING] = null;
        } else if (isEncodingOptions(options)) {
          this[ENCODING] = options.encoding;
          this[OBJECTMODE] = false;
        } else {
          this[OBJECTMODE] = false;
          this[ENCODING] = null;
        }
        this[ASYNC] = !!options.async;
        this[DECODER] = this[ENCODING] ? new import_node_string_decoder.StringDecoder(this[ENCODING]) : null;
        if (options && options.debugExposeBuffer === true) {
          Object.defineProperty(this, "buffer", { get: () => this[BUFFER] });
        }
        if (options && options.debugExposePipes === true) {
          Object.defineProperty(this, "pipes", { get: () => this[PIPES] });
        }
        const { signal } = options;
        if (signal) {
          this[SIGNAL] = signal;
          if (signal.aborted) {
            this[ABORT]();
          } else {
            signal.addEventListener("abort", () => this[ABORT]());
          }
        }
      }
      /**
       * The amount of data stored in the buffer waiting to be read.
       *
       * For Buffer strings, this will be the total byte length.
       * For string encoding streams, this will be the string character length,
       * according to JavaScript's `string.length` logic.
       * For objectMode streams, this is a count of the items waiting to be
       * emitted.
       */
      get bufferLength() {
        return this[BUFFERLENGTH];
      }
      /**
       * The `BufferEncoding` currently in use, or `null`
       */
      get encoding() {
        return this[ENCODING];
      }
      /**
       * @deprecated - This is a read only property
       */
      set encoding(_enc) {
        throw new Error("Encoding must be set at instantiation time");
      }
      /**
       * @deprecated - Encoding may only be set at instantiation time
       */
      setEncoding(_enc) {
        throw new Error("Encoding must be set at instantiation time");
      }
      /**
       * True if this is an objectMode stream
       */
      get objectMode() {
        return this[OBJECTMODE];
      }
      /**
       * @deprecated - This is a read-only property
       */
      set objectMode(_om) {
        throw new Error("objectMode must be set at instantiation time");
      }
      /**
       * true if this is an async stream
       */
      get ["async"]() {
        return this[ASYNC];
      }
      /**
       * Set to true to make this stream async.
       *
       * Once set, it cannot be unset, as this would potentially cause incorrect
       * behavior.  Ie, a sync stream can be made async, but an async stream
       * cannot be safely made sync.
       */
      set ["async"](a) {
        this[ASYNC] = this[ASYNC] || !!a;
      }
      // drop everything and get out of the flow completely
      [ABORT]() {
        this[ABORTED] = true;
        this.emit("abort", this[SIGNAL]?.reason);
        this.destroy(this[SIGNAL]?.reason);
      }
      /**
       * True if the stream has been aborted.
       */
      get aborted() {
        return this[ABORTED];
      }
      /**
       * No-op setter. Stream aborted status is set via the AbortSignal provided
       * in the constructor options.
       */
      set aborted(_) {
      }
      write(chunk, encoding, cb) {
        if (this[ABORTED])
          return false;
        if (this[EOF])
          throw new Error("write after end");
        if (this[DESTROYED]) {
          this.emit("error", Object.assign(new Error("Cannot call write after a stream was destroyed"), { code: "ERR_STREAM_DESTROYED" }));
          return true;
        }
        if (typeof encoding === "function") {
          cb = encoding;
          encoding = "utf8";
        }
        if (!encoding)
          encoding = "utf8";
        const fn2 = this[ASYNC] ? defer : nodefer;
        if (!this[OBJECTMODE] && !Buffer.isBuffer(chunk)) {
          if (isArrayBufferView(chunk)) {
            chunk = Buffer.from(chunk.buffer, chunk.byteOffset, chunk.byteLength);
          } else if (isArrayBufferLike(chunk)) {
            chunk = Buffer.from(chunk);
          } else if (typeof chunk !== "string") {
            throw new Error("Non-contiguous data written to non-objectMode stream");
          }
        }
        if (this[OBJECTMODE]) {
          if (this[FLOWING] && this[BUFFERLENGTH] !== 0)
            this[FLUSH](true);
          if (this[FLOWING])
            this.emit("data", chunk);
          else
            this[BUFFERPUSH](chunk);
          if (this[BUFFERLENGTH] !== 0)
            this.emit("readable");
          if (cb)
            fn2(cb);
          return this[FLOWING];
        }
        if (!chunk.length) {
          if (this[BUFFERLENGTH] !== 0)
            this.emit("readable");
          if (cb)
            fn2(cb);
          return this[FLOWING];
        }
        if (typeof chunk === "string" && // unless it is a string already ready for us to use
        !(encoding === this[ENCODING] && !this[DECODER]?.lastNeed)) {
          chunk = Buffer.from(chunk, encoding);
        }
        if (Buffer.isBuffer(chunk) && this[ENCODING]) {
          chunk = this[DECODER].write(chunk);
        }
        if (this[FLOWING] && this[BUFFERLENGTH] !== 0)
          this[FLUSH](true);
        if (this[FLOWING])
          this.emit("data", chunk);
        else
          this[BUFFERPUSH](chunk);
        if (this[BUFFERLENGTH] !== 0)
          this.emit("readable");
        if (cb)
          fn2(cb);
        return this[FLOWING];
      }
      /**
       * Low-level explicit read method.
       *
       * In objectMode, the argument is ignored, and one item is returned if
       * available.
       *
       * `n` is the number of bytes (or in the case of encoding streams,
       * characters) to consume. If `n` is not provided, then the entire buffer
       * is returned, or `null` is returned if no data is available.
       *
       * If `n` is greater that the amount of data in the internal buffer,
       * then `null` is returned.
       */
      read(n) {
        if (this[DESTROYED])
          return null;
        this[DISCARDED] = false;
        if (this[BUFFERLENGTH] === 0 || n === 0 || n && n > this[BUFFERLENGTH]) {
          this[MAYBE_EMIT_END]();
          return null;
        }
        if (this[OBJECTMODE])
          n = null;
        if (this[BUFFER].length > 1 && !this[OBJECTMODE]) {
          this[BUFFER] = [
            this[ENCODING] ? this[BUFFER].join("") : Buffer.concat(this[BUFFER], this[BUFFERLENGTH])
          ];
        }
        const ret = this[READ](n || null, this[BUFFER][0]);
        this[MAYBE_EMIT_END]();
        return ret;
      }
      [READ](n, chunk) {
        if (this[OBJECTMODE])
          this[BUFFERSHIFT]();
        else {
          const c = chunk;
          if (n === c.length || n === null)
            this[BUFFERSHIFT]();
          else if (typeof c === "string") {
            this[BUFFER][0] = c.slice(n);
            chunk = c.slice(0, n);
            this[BUFFERLENGTH] -= n;
          } else {
            this[BUFFER][0] = c.subarray(n);
            chunk = c.subarray(0, n);
            this[BUFFERLENGTH] -= n;
          }
        }
        this.emit("data", chunk);
        if (!this[BUFFER].length && !this[EOF])
          this.emit("drain");
        return chunk;
      }
      end(chunk, encoding, cb) {
        if (typeof chunk === "function") {
          cb = chunk;
          chunk = void 0;
        }
        if (typeof encoding === "function") {
          cb = encoding;
          encoding = "utf8";
        }
        if (chunk !== void 0)
          this.write(chunk, encoding);
        if (cb)
          this.once("end", cb);
        this[EOF] = true;
        this.writable = false;
        if (this[FLOWING] || !this[PAUSED])
          this[MAYBE_EMIT_END]();
        return this;
      }
      // don't let the internal resume be overwritten
      [RESUME]() {
        if (this[DESTROYED])
          return;
        if (!this[DATALISTENERS] && !this[PIPES].length) {
          this[DISCARDED] = true;
        }
        this[PAUSED] = false;
        this[FLOWING] = true;
        this.emit("resume");
        if (this[BUFFER].length)
          this[FLUSH]();
        else if (this[EOF])
          this[MAYBE_EMIT_END]();
        else
          this.emit("drain");
      }
      /**
       * Resume the stream if it is currently in a paused state
       *
       * If called when there are no pipe destinations or `data` event listeners,
       * this will place the stream in a "discarded" state, where all data will
       * be thrown away. The discarded state is removed if a pipe destination or
       * data handler is added, if pause() is called, or if any synchronous or
       * asynchronous iteration is started.
       */
      resume() {
        return this[RESUME]();
      }
      /**
       * Pause the stream
       */
      pause() {
        this[FLOWING] = false;
        this[PAUSED] = true;
        this[DISCARDED] = false;
      }
      /**
       * true if the stream has been forcibly destroyed
       */
      get destroyed() {
        return this[DESTROYED];
      }
      /**
       * true if the stream is currently in a flowing state, meaning that
       * any writes will be immediately emitted.
       */
      get flowing() {
        return this[FLOWING];
      }
      /**
       * true if the stream is currently in a paused state
       */
      get paused() {
        return this[PAUSED];
      }
      [BUFFERPUSH](chunk) {
        if (this[OBJECTMODE])
          this[BUFFERLENGTH] += 1;
        else
          this[BUFFERLENGTH] += chunk.length;
        this[BUFFER].push(chunk);
      }
      [BUFFERSHIFT]() {
        if (this[OBJECTMODE])
          this[BUFFERLENGTH] -= 1;
        else
          this[BUFFERLENGTH] -= this[BUFFER][0].length;
        return this[BUFFER].shift();
      }
      [FLUSH](noDrain = false) {
        do {
        } while (this[FLUSHCHUNK](this[BUFFERSHIFT]()) && this[BUFFER].length);
        if (!noDrain && !this[BUFFER].length && !this[EOF])
          this.emit("drain");
      }
      [FLUSHCHUNK](chunk) {
        this.emit("data", chunk);
        return this[FLOWING];
      }
      /**
       * Pipe all data emitted by this stream into the destination provided.
       *
       * Triggers the flow of data.
       */
      pipe(dest, opts) {
        if (this[DESTROYED])
          return dest;
        this[DISCARDED] = false;
        const ended = this[EMITTED_END];
        opts = opts || {};
        if (dest === proc.stdout || dest === proc.stderr)
          opts.end = false;
        else
          opts.end = opts.end !== false;
        opts.proxyErrors = !!opts.proxyErrors;
        if (ended) {
          if (opts.end)
            dest.end();
        } else {
          this[PIPES].push(!opts.proxyErrors ? new Pipe(this, dest, opts) : new PipeProxyErrors(this, dest, opts));
          if (this[ASYNC])
            defer(() => this[RESUME]());
          else
            this[RESUME]();
        }
        return dest;
      }
      /**
       * Fully unhook a piped destination stream.
       *
       * If the destination stream was the only consumer of this stream (ie,
       * there are no other piped destinations or `'data'` event listeners)
       * then the flow of data will stop until there is another consumer or
       * {@link Minipass#resume} is explicitly called.
       */
      unpipe(dest) {
        const p = this[PIPES].find((p2) => p2.dest === dest);
        if (p) {
          if (this[PIPES].length === 1) {
            if (this[FLOWING] && this[DATALISTENERS] === 0) {
              this[FLOWING] = false;
            }
            this[PIPES] = [];
          } else
            this[PIPES].splice(this[PIPES].indexOf(p), 1);
          p.unpipe();
        }
      }
      /**
       * Alias for {@link Minipass#on}
       */
      addListener(ev, handler) {
        return this.on(ev, handler);
      }
      /**
       * Mostly identical to `EventEmitter.on`, with the following
       * behavior differences to prevent data loss and unnecessary hangs:
       *
       * - Adding a 'data' event handler will trigger the flow of data
       *
       * - Adding a 'readable' event handler when there is data waiting to be read
       *   will cause 'readable' to be emitted immediately.
       *
       * - Adding an 'endish' event handler ('end', 'finish', etc.) which has
       *   already passed will cause the event to be emitted immediately and all
       *   handlers removed.
       *
       * - Adding an 'error' event handler after an error has been emitted will
       *   cause the event to be re-emitted immediately with the error previously
       *   raised.
       */
      on(ev, handler) {
        const ret = super.on(ev, handler);
        if (ev === "data") {
          this[DISCARDED] = false;
          this[DATALISTENERS]++;
          if (!this[PIPES].length && !this[FLOWING]) {
            this[RESUME]();
          }
        } else if (ev === "readable" && this[BUFFERLENGTH] !== 0) {
          super.emit("readable");
        } else if (isEndish(ev) && this[EMITTED_END]) {
          super.emit(ev);
          this.removeAllListeners(ev);
        } else if (ev === "error" && this[EMITTED_ERROR]) {
          const h = handler;
          if (this[ASYNC])
            defer(() => h.call(this, this[EMITTED_ERROR]));
          else
            h.call(this, this[EMITTED_ERROR]);
        }
        return ret;
      }
      /**
       * Alias for {@link Minipass#off}
       */
      removeListener(ev, handler) {
        return this.off(ev, handler);
      }
      /**
       * Mostly identical to `EventEmitter.off`
       *
       * If a 'data' event handler is removed, and it was the last consumer
       * (ie, there are no pipe destinations or other 'data' event listeners),
       * then the flow of data will stop until there is another consumer or
       * {@link Minipass#resume} is explicitly called.
       */
      off(ev, handler) {
        const ret = super.off(ev, handler);
        if (ev === "data") {
          this[DATALISTENERS] = this.listeners("data").length;
          if (this[DATALISTENERS] === 0 && !this[DISCARDED] && !this[PIPES].length) {
            this[FLOWING] = false;
          }
        }
        return ret;
      }
      /**
       * Mostly identical to `EventEmitter.removeAllListeners`
       *
       * If all 'data' event handlers are removed, and they were the last consumer
       * (ie, there are no pipe destinations), then the flow of data will stop
       * until there is another consumer or {@link Minipass#resume} is explicitly
       * called.
       */
      removeAllListeners(ev) {
        const ret = super.removeAllListeners(ev);
        if (ev === "data" || ev === void 0) {
          this[DATALISTENERS] = 0;
          if (!this[DISCARDED] && !this[PIPES].length) {
            this[FLOWING] = false;
          }
        }
        return ret;
      }
      /**
       * true if the 'end' event has been emitted
       */
      get emittedEnd() {
        return this[EMITTED_END];
      }
      [MAYBE_EMIT_END]() {
        if (!this[EMITTING_END] && !this[EMITTED_END] && !this[DESTROYED] && this[BUFFER].length === 0 && this[EOF]) {
          this[EMITTING_END] = true;
          this.emit("end");
          this.emit("prefinish");
          this.emit("finish");
          if (this[CLOSED])
            this.emit("close");
          this[EMITTING_END] = false;
        }
      }
      /**
       * Mostly identical to `EventEmitter.emit`, with the following
       * behavior differences to prevent data loss and unnecessary hangs:
       *
       * If the stream has been destroyed, and the event is something other
       * than 'close' or 'error', then `false` is returned and no handlers
       * are called.
       *
       * If the event is 'end', and has already been emitted, then the event
       * is ignored. If the stream is in a paused or non-flowing state, then
       * the event will be deferred until data flow resumes. If the stream is
       * async, then handlers will be called on the next tick rather than
       * immediately.
       *
       * If the event is 'close', and 'end' has not yet been emitted, then
       * the event will be deferred until after 'end' is emitted.
       *
       * If the event is 'error', and an AbortSignal was provided for the stream,
       * and there are no listeners, then the event is ignored, matching the
       * behavior of node core streams in the presense of an AbortSignal.
       *
       * If the event is 'finish' or 'prefinish', then all listeners will be
       * removed after emitting the event, to prevent double-firing.
       */
      emit(ev, ...args) {
        const data = args[0];
        if (ev !== "error" && ev !== "close" && ev !== DESTROYED && this[DESTROYED]) {
          return false;
        } else if (ev === "data") {
          return !this[OBJECTMODE] && !data ? false : this[ASYNC] ? (defer(() => this[EMITDATA](data)), true) : this[EMITDATA](data);
        } else if (ev === "end") {
          return this[EMITEND]();
        } else if (ev === "close") {
          this[CLOSED] = true;
          if (!this[EMITTED_END] && !this[DESTROYED])
            return false;
          const ret2 = super.emit("close");
          this.removeAllListeners("close");
          return ret2;
        } else if (ev === "error") {
          this[EMITTED_ERROR] = data;
          super.emit(ERROR, data);
          const ret2 = !this[SIGNAL] || this.listeners("error").length ? super.emit("error", data) : false;
          this[MAYBE_EMIT_END]();
          return ret2;
        } else if (ev === "resume") {
          const ret2 = super.emit("resume");
          this[MAYBE_EMIT_END]();
          return ret2;
        } else if (ev === "finish" || ev === "prefinish") {
          const ret2 = super.emit(ev);
          this.removeAllListeners(ev);
          return ret2;
        }
        const ret = super.emit(ev, ...args);
        this[MAYBE_EMIT_END]();
        return ret;
      }
      [EMITDATA](data) {
        for (const p of this[PIPES]) {
          if (p.dest.write(data) === false)
            this.pause();
        }
        const ret = this[DISCARDED] ? false : super.emit("data", data);
        this[MAYBE_EMIT_END]();
        return ret;
      }
      [EMITEND]() {
        if (this[EMITTED_END])
          return false;
        this[EMITTED_END] = true;
        this.readable = false;
        return this[ASYNC] ? (defer(() => this[EMITEND2]()), true) : this[EMITEND2]();
      }
      [EMITEND2]() {
        if (this[DECODER]) {
          const data = this[DECODER].end();
          if (data) {
            for (const p of this[PIPES]) {
              p.dest.write(data);
            }
            if (!this[DISCARDED])
              super.emit("data", data);
          }
        }
        for (const p of this[PIPES]) {
          p.end();
        }
        const ret = super.emit("end");
        this.removeAllListeners("end");
        return ret;
      }
      /**
       * Return a Promise that resolves to an array of all emitted data once
       * the stream ends.
       */
      async collect() {
        const buf = Object.assign([], {
          dataLength: 0
        });
        if (!this[OBJECTMODE])
          buf.dataLength = 0;
        const p = this.promise();
        this.on("data", (c) => {
          buf.push(c);
          if (!this[OBJECTMODE])
            buf.dataLength += c.length;
        });
        await p;
        return buf;
      }
      /**
       * Return a Promise that resolves to the concatenation of all emitted data
       * once the stream ends.
       *
       * Not allowed on objectMode streams.
       */
      async concat() {
        if (this[OBJECTMODE]) {
          throw new Error("cannot concat in objectMode");
        }
        const buf = await this.collect();
        return this[ENCODING] ? buf.join("") : Buffer.concat(buf, buf.dataLength);
      }
      /**
       * Return a void Promise that resolves once the stream ends.
       */
      async promise() {
        return new Promise((resolve, reject) => {
          this.on(DESTROYED, () => reject(new Error("stream destroyed")));
          this.on("error", (er) => reject(er));
          this.on("end", () => resolve());
        });
      }
      /**
       * Asynchronous `for await of` iteration.
       *
       * This will continue emitting all chunks until the stream terminates.
       */
      [Symbol.asyncIterator]() {
        this[DISCARDED] = false;
        let stopped = false;
        const stop = async () => {
          this.pause();
          stopped = true;
          return { value: void 0, done: true };
        };
        const next = () => {
          if (stopped)
            return stop();
          const res = this.read();
          if (res !== null)
            return Promise.resolve({ done: false, value: res });
          if (this[EOF])
            return stop();
          let resolve;
          let reject;
          const onerr = (er) => {
            this.off("data", ondata);
            this.off("end", onend);
            this.off(DESTROYED, ondestroy);
            stop();
            reject(er);
          };
          const ondata = (value) => {
            this.off("error", onerr);
            this.off("end", onend);
            this.off(DESTROYED, ondestroy);
            this.pause();
            resolve({ value, done: !!this[EOF] });
          };
          const onend = () => {
            this.off("error", onerr);
            this.off("data", ondata);
            this.off(DESTROYED, ondestroy);
            stop();
            resolve({ done: true, value: void 0 });
          };
          const ondestroy = () => onerr(new Error("stream destroyed"));
          return new Promise((res2, rej) => {
            reject = rej;
            resolve = res2;
            this.once(DESTROYED, ondestroy);
            this.once("error", onerr);
            this.once("end", onend);
            this.once("data", ondata);
          });
        };
        return {
          next,
          throw: stop,
          return: stop,
          [Symbol.asyncIterator]() {
            return this;
          },
          [Symbol.asyncDispose]: async () => {
          }
        };
      }
      /**
       * Synchronous `for of` iteration.
       *
       * The iteration will terminate when the internal buffer runs out, even
       * if the stream has not yet terminated.
       */
      [Symbol.iterator]() {
        this[DISCARDED] = false;
        let stopped = false;
        const stop = () => {
          this.pause();
          this.off(ERROR, stop);
          this.off(DESTROYED, stop);
          this.off("end", stop);
          stopped = true;
          return { done: true, value: void 0 };
        };
        const next = () => {
          if (stopped)
            return stop();
          const value = this.read();
          return value === null ? stop() : { done: false, value };
        };
        this.once("end", stop);
        this.once(ERROR, stop);
        this.once(DESTROYED, stop);
        return {
          next,
          throw: stop,
          return: stop,
          [Symbol.iterator]() {
            return this;
          },
          [Symbol.dispose]: () => {
          }
        };
      }
      /**
       * Destroy a stream, preventing it from being used for any further purpose.
       *
       * If the stream has a `close()` method, then it will be called on
       * destruction.
       *
       * After destruction, any attempt to write data, read data, or emit most
       * events will be ignored.
       *
       * If an error argument is provided, then it will be emitted in an
       * 'error' event.
       */
      destroy(er) {
        if (this[DESTROYED]) {
          if (er)
            this.emit("error", er);
          else
            this.emit(DESTROYED);
          return this;
        }
        this[DESTROYED] = true;
        this[DISCARDED] = true;
        this[BUFFER].length = 0;
        this[BUFFERLENGTH] = 0;
        const wc = this;
        if (typeof wc.close === "function" && !this[CLOSED])
          wc.close();
        if (er)
          this.emit("error", er);
        else
          this.emit(DESTROYED);
        return this;
      }
      /**
       * Alias for {@link isStream}
       *
       * Former export location, maintained for backwards compatibility.
       *
       * @deprecated
       */
      static get isStream() {
        return isStream;
      }
    };
  }
});

// node_modules/@isaacs/fs-minipass/dist/esm/index.js
var import_events2, import_fs2, writev, _autoClose, _close, _ended, _fd, _finished, _flags, _flush, _handleChunk, _makeBuf, _mode, _needDrain, _onerror, _onopen, _onread, _onwrite, _open, _path, _pos, _queue, _read, _readSize, _reading, _remain, _size, _write, _writing, _defaultFlag, _errored, ReadStream, ReadStreamSync, WriteStream, WriteStreamSync;
var init_esm2 = __esm({
  "node_modules/@isaacs/fs-minipass/dist/esm/index.js"() {
    import_events2 = __toESM(require("events"), 1);
    import_fs2 = __toESM(require("fs"), 1);
    init_esm();
    writev = import_fs2.default.writev;
    _autoClose = /* @__PURE__ */ Symbol("_autoClose");
    _close = /* @__PURE__ */ Symbol("_close");
    _ended = /* @__PURE__ */ Symbol("_ended");
    _fd = /* @__PURE__ */ Symbol("_fd");
    _finished = /* @__PURE__ */ Symbol("_finished");
    _flags = /* @__PURE__ */ Symbol("_flags");
    _flush = /* @__PURE__ */ Symbol("_flush");
    _handleChunk = /* @__PURE__ */ Symbol("_handleChunk");
    _makeBuf = /* @__PURE__ */ Symbol("_makeBuf");
    _mode = /* @__PURE__ */ Symbol("_mode");
    _needDrain = /* @__PURE__ */ Symbol("_needDrain");
    _onerror = /* @__PURE__ */ Symbol("_onerror");
    _onopen = /* @__PURE__ */ Symbol("_onopen");
    _onread = /* @__PURE__ */ Symbol("_onread");
    _onwrite = /* @__PURE__ */ Symbol("_onwrite");
    _open = /* @__PURE__ */ Symbol("_open");
    _path = /* @__PURE__ */ Symbol("_path");
    _pos = /* @__PURE__ */ Symbol("_pos");
    _queue = /* @__PURE__ */ Symbol("_queue");
    _read = /* @__PURE__ */ Symbol("_read");
    _readSize = /* @__PURE__ */ Symbol("_readSize");
    _reading = /* @__PURE__ */ Symbol("_reading");
    _remain = /* @__PURE__ */ Symbol("_remain");
    _size = /* @__PURE__ */ Symbol("_size");
    _write = /* @__PURE__ */ Symbol("_write");
    _writing = /* @__PURE__ */ Symbol("_writing");
    _defaultFlag = /* @__PURE__ */ Symbol("_defaultFlag");
    _errored = /* @__PURE__ */ Symbol("_errored");
    ReadStream = class extends Minipass {
      [_errored] = false;
      [_fd];
      [_path];
      [_readSize];
      [_reading] = false;
      [_size];
      [_remain];
      [_autoClose];
      constructor(path16, opt) {
        opt = opt || {};
        super(opt);
        this.readable = true;
        this.writable = false;
        if (typeof path16 !== "string") {
          throw new TypeError("path must be a string");
        }
        this[_errored] = false;
        this[_fd] = typeof opt.fd === "number" ? opt.fd : void 0;
        this[_path] = path16;
        this[_readSize] = opt.readSize || 16 * 1024 * 1024;
        this[_reading] = false;
        this[_size] = typeof opt.size === "number" ? opt.size : Infinity;
        this[_remain] = this[_size];
        this[_autoClose] = typeof opt.autoClose === "boolean" ? opt.autoClose : true;
        if (typeof this[_fd] === "number") {
          this[_read]();
        } else {
          this[_open]();
        }
      }
      get fd() {
        return this[_fd];
      }
      get path() {
        return this[_path];
      }
      //@ts-ignore
      write() {
        throw new TypeError("this is a readable stream");
      }
      //@ts-ignore
      end() {
        throw new TypeError("this is a readable stream");
      }
      [_open]() {
        import_fs2.default.open(this[_path], "r", (er, fd) => this[_onopen](er, fd));
      }
      [_onopen](er, fd) {
        if (er) {
          this[_onerror](er);
        } else {
          this[_fd] = fd;
          this.emit("open", fd);
          this[_read]();
        }
      }
      [_makeBuf]() {
        return Buffer.allocUnsafe(Math.min(this[_readSize], this[_remain]));
      }
      [_read]() {
        if (!this[_reading]) {
          this[_reading] = true;
          const buf = this[_makeBuf]();
          if (buf.length === 0) {
            return process.nextTick(() => this[_onread](null, 0, buf));
          }
          import_fs2.default.read(this[_fd], buf, 0, buf.length, null, (er, br, b) => this[_onread](er, br, b));
        }
      }
      [_onread](er, br, buf) {
        this[_reading] = false;
        if (er) {
          this[_onerror](er);
        } else if (this[_handleChunk](br, buf)) {
          this[_read]();
        }
      }
      [_close]() {
        if (this[_autoClose] && typeof this[_fd] === "number") {
          const fd = this[_fd];
          this[_fd] = void 0;
          import_fs2.default.close(fd, (er) => er ? this.emit("error", er) : this.emit("close"));
        }
      }
      [_onerror](er) {
        this[_reading] = true;
        this[_close]();
        this.emit("error", er);
      }
      [_handleChunk](br, buf) {
        let ret = false;
        this[_remain] -= br;
        if (br > 0) {
          ret = super.write(br < buf.length ? buf.subarray(0, br) : buf);
        }
        if (br === 0 || this[_remain] <= 0) {
          ret = false;
          this[_close]();
          super.end();
        }
        return ret;
      }
      emit(ev, ...args) {
        switch (ev) {
          case "prefinish":
          case "finish":
            return false;
          case "drain":
            if (typeof this[_fd] === "number") {
              this[_read]();
            }
            return false;
          case "error":
            if (this[_errored]) {
              return false;
            }
            this[_errored] = true;
            return super.emit(ev, ...args);
          default:
            return super.emit(ev, ...args);
        }
      }
    };
    ReadStreamSync = class extends ReadStream {
      [_open]() {
        let threw = true;
        try {
          this[_onopen](null, import_fs2.default.openSync(this[_path], "r"));
          threw = false;
        } finally {
          if (threw) {
            this[_close]();
          }
        }
      }
      [_read]() {
        let threw = true;
        try {
          if (!this[_reading]) {
            this[_reading] = true;
            do {
              const buf = this[_makeBuf]();
              const br = buf.length === 0 ? 0 : import_fs2.default.readSync(this[_fd], buf, 0, buf.length, null);
              if (!this[_handleChunk](br, buf)) {
                break;
              }
            } while (true);
            this[_reading] = false;
          }
          threw = false;
        } finally {
          if (threw) {
            this[_close]();
          }
        }
      }
      [_close]() {
        if (this[_autoClose] && typeof this[_fd] === "number") {
          const fd = this[_fd];
          this[_fd] = void 0;
          import_fs2.default.closeSync(fd);
          this.emit("close");
        }
      }
    };
    WriteStream = class extends import_events2.default {
      readable = false;
      writable = true;
      [_errored] = false;
      [_writing] = false;
      [_ended] = false;
      [_queue] = [];
      [_needDrain] = false;
      [_path];
      [_mode];
      [_autoClose];
      [_fd];
      [_defaultFlag];
      [_flags];
      [_finished] = false;
      [_pos];
      constructor(path16, opt) {
        opt = opt || {};
        super(opt);
        this[_path] = path16;
        this[_fd] = typeof opt.fd === "number" ? opt.fd : void 0;
        this[_mode] = opt.mode === void 0 ? 438 : opt.mode;
        this[_pos] = typeof opt.start === "number" ? opt.start : void 0;
        this[_autoClose] = typeof opt.autoClose === "boolean" ? opt.autoClose : true;
        const defaultFlag = this[_pos] !== void 0 ? "r+" : "w";
        this[_defaultFlag] = opt.flags === void 0;
        this[_flags] = opt.flags === void 0 ? defaultFlag : opt.flags;
        if (this[_fd] === void 0) {
          this[_open]();
        }
      }
      emit(ev, ...args) {
        if (ev === "error") {
          if (this[_errored]) {
            return false;
          }
          this[_errored] = true;
        }
        return super.emit(ev, ...args);
      }
      get fd() {
        return this[_fd];
      }
      get path() {
        return this[_path];
      }
      [_onerror](er) {
        this[_close]();
        this[_writing] = true;
        this.emit("error", er);
      }
      [_open]() {
        import_fs2.default.open(this[_path], this[_flags], this[_mode], (er, fd) => this[_onopen](er, fd));
      }
      [_onopen](er, fd) {
        if (this[_defaultFlag] && this[_flags] === "r+" && er && er.code === "ENOENT") {
          this[_flags] = "w";
          this[_open]();
        } else if (er) {
          this[_onerror](er);
        } else {
          this[_fd] = fd;
          this.emit("open", fd);
          if (!this[_writing]) {
            this[_flush]();
          }
        }
      }
      end(buf, enc) {
        if (buf) {
          this.write(buf, enc);
        }
        this[_ended] = true;
        if (!this[_writing] && !this[_queue].length && typeof this[_fd] === "number") {
          this[_onwrite](null, 0);
        }
        return this;
      }
      write(buf, enc) {
        if (typeof buf === "string") {
          buf = Buffer.from(buf, enc);
        }
        if (this[_ended]) {
          this.emit("error", new Error("write() after end()"));
          return false;
        }
        if (this[_fd] === void 0 || this[_writing] || this[_queue].length) {
          this[_queue].push(buf);
          this[_needDrain] = true;
          return false;
        }
        this[_writing] = true;
        this[_write](buf);
        return true;
      }
      [_write](buf) {
        import_fs2.default.write(this[_fd], buf, 0, buf.length, this[_pos], (er, bw) => this[_onwrite](er, bw));
      }
      [_onwrite](er, bw) {
        if (er) {
          this[_onerror](er);
        } else {
          if (this[_pos] !== void 0 && typeof bw === "number") {
            this[_pos] += bw;
          }
          if (this[_queue].length) {
            this[_flush]();
          } else {
            this[_writing] = false;
            if (this[_ended] && !this[_finished]) {
              this[_finished] = true;
              this[_close]();
              this.emit("finish");
            } else if (this[_needDrain]) {
              this[_needDrain] = false;
              this.emit("drain");
            }
          }
        }
      }
      [_flush]() {
        if (this[_queue].length === 0) {
          if (this[_ended]) {
            this[_onwrite](null, 0);
          }
        } else if (this[_queue].length === 1) {
          this[_write](this[_queue].pop());
        } else {
          const iovec = this[_queue];
          this[_queue] = [];
          writev(this[_fd], iovec, this[_pos], (er, bw) => this[_onwrite](er, bw));
        }
      }
      [_close]() {
        if (this[_autoClose] && typeof this[_fd] === "number") {
          const fd = this[_fd];
          this[_fd] = void 0;
          import_fs2.default.close(fd, (er) => er ? this.emit("error", er) : this.emit("close"));
        }
      }
    };
    WriteStreamSync = class extends WriteStream {
      [_open]() {
        let fd;
        if (this[_defaultFlag] && this[_flags] === "r+") {
          try {
            fd = import_fs2.default.openSync(this[_path], this[_flags], this[_mode]);
          } catch (er) {
            if (er?.code === "ENOENT") {
              this[_flags] = "w";
              return this[_open]();
            } else {
              throw er;
            }
          }
        } else {
          fd = import_fs2.default.openSync(this[_path], this[_flags], this[_mode]);
        }
        this[_onopen](null, fd);
      }
      [_close]() {
        if (this[_autoClose] && typeof this[_fd] === "number") {
          const fd = this[_fd];
          this[_fd] = void 0;
          import_fs2.default.closeSync(fd);
          this.emit("close");
        }
      }
      [_write](buf) {
        let threw = true;
        try {
          this[_onwrite](null, import_fs2.default.writeSync(this[_fd], buf, 0, buf.length, this[_pos]));
          threw = false;
        } finally {
          if (threw) {
            try {
              this[_close]();
            } catch {
            }
          }
        }
      }
    };
  }
});

// node_modules/tar/dist/esm/options.js
var argmap, isSyncFile, isAsyncFile, isSyncNoFile, isAsyncNoFile, dealiasKey, dealias;
var init_options = __esm({
  "node_modules/tar/dist/esm/options.js"() {
    argmap = /* @__PURE__ */ new Map([
      ["C", "cwd"],
      ["f", "file"],
      ["z", "gzip"],
      ["P", "preservePaths"],
      ["U", "unlink"],
      ["strip-components", "strip"],
      ["stripComponents", "strip"],
      ["keep-newer", "newer"],
      ["keepNewer", "newer"],
      ["keep-newer-files", "newer"],
      ["keepNewerFiles", "newer"],
      ["k", "keep"],
      ["keep-existing", "keep"],
      ["keepExisting", "keep"],
      ["m", "noMtime"],
      ["no-mtime", "noMtime"],
      ["p", "preserveOwner"],
      ["L", "follow"],
      ["h", "follow"],
      ["onentry", "onReadEntry"]
    ]);
    isSyncFile = (o) => !!o.sync && !!o.file;
    isAsyncFile = (o) => !o.sync && !!o.file;
    isSyncNoFile = (o) => !!o.sync && !o.file;
    isAsyncNoFile = (o) => !o.sync && !o.file;
    dealiasKey = (k) => {
      const d = argmap.get(k);
      if (d)
        return d;
      return k;
    };
    dealias = (opt = {}) => {
      if (!opt)
        return {};
      const result = {};
      for (const [key, v] of Object.entries(opt)) {
        const k = dealiasKey(key);
        result[k] = v;
      }
      if (result.chmod === void 0 && result.noChmod === false) {
        result.chmod = true;
      }
      delete result.noChmod;
      return result;
    };
  }
});

// node_modules/tar/dist/esm/make-command.js
var makeCommand;
var init_make_command = __esm({
  "node_modules/tar/dist/esm/make-command.js"() {
    init_options();
    makeCommand = (syncFile, asyncFile, syncNoFile, asyncNoFile, validate) => {
      return Object.assign((opt_ = [], entries, cb) => {
        if (Array.isArray(opt_)) {
          entries = opt_;
          opt_ = {};
        }
        if (typeof entries === "function") {
          cb = entries;
          entries = void 0;
        }
        entries = !entries ? [] : Array.from(entries);
        const opt = dealias(opt_);
        validate?.(opt, entries);
        if (isSyncFile(opt)) {
          if (typeof cb === "function") {
            throw new TypeError("callback not supported for sync tar functions");
          }
          return syncFile(opt, entries);
        } else if (isAsyncFile(opt)) {
          const p = asyncFile(opt, entries);
          return cb ? p.then(() => cb(), cb) : p;
        } else if (isSyncNoFile(opt)) {
          if (typeof cb === "function") {
            throw new TypeError("callback not supported for sync tar functions");
          }
          return syncNoFile(opt, entries);
        } else if (isAsyncNoFile(opt)) {
          if (typeof cb === "function") {
            throw new TypeError("callback only supported with file option");
          }
          return asyncNoFile(opt, entries);
        }
        throw new Error("impossible options??");
      }, {
        syncFile,
        asyncFile,
        syncNoFile,
        asyncNoFile,
        validate
      });
    };
  }
});

// node_modules/minizlib/dist/esm/constants.js
var import_zlib, realZlibConstants, constants;
var init_constants = __esm({
  "node_modules/minizlib/dist/esm/constants.js"() {
    import_zlib = __toESM(require("zlib"), 1);
    realZlibConstants = import_zlib.default.constants || { ZLIB_VERNUM: 4736 };
    constants = Object.freeze(Object.assign(/* @__PURE__ */ Object.create(null), {
      Z_NO_FLUSH: 0,
      Z_PARTIAL_FLUSH: 1,
      Z_SYNC_FLUSH: 2,
      Z_FULL_FLUSH: 3,
      Z_FINISH: 4,
      Z_BLOCK: 5,
      Z_OK: 0,
      Z_STREAM_END: 1,
      Z_NEED_DICT: 2,
      Z_ERRNO: -1,
      Z_STREAM_ERROR: -2,
      Z_DATA_ERROR: -3,
      Z_MEM_ERROR: -4,
      Z_BUF_ERROR: -5,
      Z_VERSION_ERROR: -6,
      Z_NO_COMPRESSION: 0,
      Z_BEST_SPEED: 1,
      Z_BEST_COMPRESSION: 9,
      Z_DEFAULT_COMPRESSION: -1,
      Z_FILTERED: 1,
      Z_HUFFMAN_ONLY: 2,
      Z_RLE: 3,
      Z_FIXED: 4,
      Z_DEFAULT_STRATEGY: 0,
      DEFLATE: 1,
      INFLATE: 2,
      GZIP: 3,
      GUNZIP: 4,
      DEFLATERAW: 5,
      INFLATERAW: 6,
      UNZIP: 7,
      BROTLI_DECODE: 8,
      BROTLI_ENCODE: 9,
      Z_MIN_WINDOWBITS: 8,
      Z_MAX_WINDOWBITS: 15,
      Z_DEFAULT_WINDOWBITS: 15,
      Z_MIN_CHUNK: 64,
      Z_MAX_CHUNK: Infinity,
      Z_DEFAULT_CHUNK: 16384,
      Z_MIN_MEMLEVEL: 1,
      Z_MAX_MEMLEVEL: 9,
      Z_DEFAULT_MEMLEVEL: 8,
      Z_MIN_LEVEL: -1,
      Z_MAX_LEVEL: 9,
      Z_DEFAULT_LEVEL: -1,
      BROTLI_OPERATION_PROCESS: 0,
      BROTLI_OPERATION_FLUSH: 1,
      BROTLI_OPERATION_FINISH: 2,
      BROTLI_OPERATION_EMIT_METADATA: 3,
      BROTLI_MODE_GENERIC: 0,
      BROTLI_MODE_TEXT: 1,
      BROTLI_MODE_FONT: 2,
      BROTLI_DEFAULT_MODE: 0,
      BROTLI_MIN_QUALITY: 0,
      BROTLI_MAX_QUALITY: 11,
      BROTLI_DEFAULT_QUALITY: 11,
      BROTLI_MIN_WINDOW_BITS: 10,
      BROTLI_MAX_WINDOW_BITS: 24,
      BROTLI_LARGE_MAX_WINDOW_BITS: 30,
      BROTLI_DEFAULT_WINDOW: 22,
      BROTLI_MIN_INPUT_BLOCK_BITS: 16,
      BROTLI_MAX_INPUT_BLOCK_BITS: 24,
      BROTLI_PARAM_MODE: 0,
      BROTLI_PARAM_QUALITY: 1,
      BROTLI_PARAM_LGWIN: 2,
      BROTLI_PARAM_LGBLOCK: 3,
      BROTLI_PARAM_DISABLE_LITERAL_CONTEXT_MODELING: 4,
      BROTLI_PARAM_SIZE_HINT: 5,
      BROTLI_PARAM_LARGE_WINDOW: 6,
      BROTLI_PARAM_NPOSTFIX: 7,
      BROTLI_PARAM_NDIRECT: 8,
      BROTLI_DECODER_RESULT_ERROR: 0,
      BROTLI_DECODER_RESULT_SUCCESS: 1,
      BROTLI_DECODER_RESULT_NEEDS_MORE_INPUT: 2,
      BROTLI_DECODER_RESULT_NEEDS_MORE_OUTPUT: 3,
      BROTLI_DECODER_PARAM_DISABLE_RING_BUFFER_REALLOCATION: 0,
      BROTLI_DECODER_PARAM_LARGE_WINDOW: 1,
      BROTLI_DECODER_NO_ERROR: 0,
      BROTLI_DECODER_SUCCESS: 1,
      BROTLI_DECODER_NEEDS_MORE_INPUT: 2,
      BROTLI_DECODER_NEEDS_MORE_OUTPUT: 3,
      BROTLI_DECODER_ERROR_FORMAT_EXUBERANT_NIBBLE: -1,
      BROTLI_DECODER_ERROR_FORMAT_RESERVED: -2,
      BROTLI_DECODER_ERROR_FORMAT_EXUBERANT_META_NIBBLE: -3,
      BROTLI_DECODER_ERROR_FORMAT_SIMPLE_HUFFMAN_ALPHABET: -4,
      BROTLI_DECODER_ERROR_FORMAT_SIMPLE_HUFFMAN_SAME: -5,
      BROTLI_DECODER_ERROR_FORMAT_CL_SPACE: -6,
      BROTLI_DECODER_ERROR_FORMAT_HUFFMAN_SPACE: -7,
      BROTLI_DECODER_ERROR_FORMAT_CONTEXT_MAP_REPEAT: -8,
      BROTLI_DECODER_ERROR_FORMAT_BLOCK_LENGTH_1: -9,
      BROTLI_DECODER_ERROR_FORMAT_BLOCK_LENGTH_2: -10,
      BROTLI_DECODER_ERROR_FORMAT_TRANSFORM: -11,
      BROTLI_DECODER_ERROR_FORMAT_DICTIONARY: -12,
      BROTLI_DECODER_ERROR_FORMAT_WINDOW_BITS: -13,
      BROTLI_DECODER_ERROR_FORMAT_PADDING_1: -14,
      BROTLI_DECODER_ERROR_FORMAT_PADDING_2: -15,
      BROTLI_DECODER_ERROR_FORMAT_DISTANCE: -16,
      BROTLI_DECODER_ERROR_DICTIONARY_NOT_SET: -19,
      BROTLI_DECODER_ERROR_INVALID_ARGUMENTS: -20,
      BROTLI_DECODER_ERROR_ALLOC_CONTEXT_MODES: -21,
      BROTLI_DECODER_ERROR_ALLOC_TREE_GROUPS: -22,
      BROTLI_DECODER_ERROR_ALLOC_CONTEXT_MAP: -25,
      BROTLI_DECODER_ERROR_ALLOC_RING_BUFFER_1: -26,
      BROTLI_DECODER_ERROR_ALLOC_RING_BUFFER_2: -27,
      BROTLI_DECODER_ERROR_ALLOC_BLOCK_TYPE_TREES: -30,
      BROTLI_DECODER_ERROR_UNREACHABLE: -31
    }, realZlibConstants));
  }
});

// node_modules/minizlib/dist/esm/index.js
var import_assert2, import_buffer, realZlib2, OriginalBufferConcat, desc, noop, passthroughBufferConcat, _superWrite, ZlibError, _flushFlag, ZlibBase, Zlib, Gzip, Unzip, Brotli, BrotliCompress, BrotliDecompress, Zstd, ZstdCompress, ZstdDecompress;
var init_esm3 = __esm({
  "node_modules/minizlib/dist/esm/index.js"() {
    import_assert2 = __toESM(require("assert"), 1);
    import_buffer = require("buffer");
    init_esm();
    realZlib2 = __toESM(require("zlib"), 1);
    init_constants();
    init_constants();
    OriginalBufferConcat = import_buffer.Buffer.concat;
    desc = Object.getOwnPropertyDescriptor(import_buffer.Buffer, "concat");
    noop = (args) => args;
    passthroughBufferConcat = desc?.writable === true || desc?.set !== void 0 ? (makeNoOp) => {
      import_buffer.Buffer.concat = makeNoOp ? noop : OriginalBufferConcat;
    } : (_) => {
    };
    _superWrite = /* @__PURE__ */ Symbol("_superWrite");
    ZlibError = class extends Error {
      code;
      errno;
      constructor(err, origin) {
        super("zlib: " + err.message, { cause: err });
        this.code = err.code;
        this.errno = err.errno;
        if (!this.code)
          this.code = "ZLIB_ERROR";
        this.message = "zlib: " + err.message;
        Error.captureStackTrace(this, origin ?? this.constructor);
      }
      get name() {
        return "ZlibError";
      }
    };
    _flushFlag = /* @__PURE__ */ Symbol("flushFlag");
    ZlibBase = class extends Minipass {
      #sawError = false;
      #ended = false;
      #flushFlag;
      #finishFlushFlag;
      #fullFlushFlag;
      #handle;
      #onError;
      get sawError() {
        return this.#sawError;
      }
      get handle() {
        return this.#handle;
      }
      /* c8 ignore start */
      get flushFlag() {
        return this.#flushFlag;
      }
      /* c8 ignore stop */
      constructor(opts, mode) {
        if (!opts || typeof opts !== "object")
          throw new TypeError("invalid options for ZlibBase constructor");
        super(opts);
        this.#flushFlag = opts.flush ?? 0;
        this.#finishFlushFlag = opts.finishFlush ?? 0;
        this.#fullFlushFlag = opts.fullFlushFlag ?? 0;
        if (typeof realZlib2[mode] !== "function") {
          throw new TypeError("Compression method not supported: " + mode);
        }
        try {
          this.#handle = new realZlib2[mode](opts);
        } catch (er) {
          throw new ZlibError(er, this.constructor);
        }
        this.#onError = (err) => {
          if (this.#sawError)
            return;
          this.#sawError = true;
          this.close();
          this.emit("error", err);
        };
        this.#handle?.on("error", (er) => this.#onError(new ZlibError(er)));
        this.once("end", () => this.close);
      }
      close() {
        if (this.#handle) {
          this.#handle.close();
          this.#handle = void 0;
          this.emit("close");
        }
      }
      reset() {
        if (!this.#sawError) {
          (0, import_assert2.default)(this.#handle, "zlib binding closed");
          return this.#handle.reset?.();
        }
      }
      flush(flushFlag) {
        if (this.ended)
          return;
        if (typeof flushFlag !== "number")
          flushFlag = this.#fullFlushFlag;
        this.write(Object.assign(import_buffer.Buffer.alloc(0), { [_flushFlag]: flushFlag }));
      }
      end(chunk, encoding, cb) {
        if (typeof chunk === "function") {
          cb = chunk;
          encoding = void 0;
          chunk = void 0;
        }
        if (typeof encoding === "function") {
          cb = encoding;
          encoding = void 0;
        }
        if (chunk) {
          if (encoding)
            this.write(chunk, encoding);
          else
            this.write(chunk);
        }
        this.flush(this.#finishFlushFlag);
        this.#ended = true;
        return super.end(cb);
      }
      get ended() {
        return this.#ended;
      }
      // overridden in the gzip classes to do portable writes
      [_superWrite](data) {
        return super.write(data);
      }
      write(chunk, encoding, cb) {
        if (typeof encoding === "function")
          cb = encoding, encoding = "utf8";
        if (typeof chunk === "string")
          chunk = import_buffer.Buffer.from(chunk, encoding);
        if (this.#sawError)
          return;
        (0, import_assert2.default)(this.#handle, "zlib binding closed");
        const nativeHandle = this.#handle._handle;
        const originalNativeClose = nativeHandle.close;
        nativeHandle.close = () => {
        };
        const originalClose = this.#handle.close;
        this.#handle.close = () => {
        };
        passthroughBufferConcat(true);
        let result = void 0;
        try {
          const flushFlag = typeof chunk[_flushFlag] === "number" ? chunk[_flushFlag] : this.#flushFlag;
          result = this.#handle._processChunk(chunk, flushFlag);
          passthroughBufferConcat(false);
        } catch (err) {
          passthroughBufferConcat(false);
          this.#onError(new ZlibError(err, this.write));
        } finally {
          if (this.#handle) {
            ;
            this.#handle._handle = nativeHandle;
            nativeHandle.close = originalNativeClose;
            this.#handle.close = originalClose;
            this.#handle.removeAllListeners("error");
          }
        }
        if (this.#handle)
          this.#handle.on("error", (er) => this.#onError(new ZlibError(er, this.write)));
        let writeReturn;
        if (result) {
          if (Array.isArray(result) && result.length > 0) {
            const r = result[0];
            writeReturn = this[_superWrite](import_buffer.Buffer.from(r));
            for (let i = 1; i < result.length; i++) {
              writeReturn = this[_superWrite](result[i]);
            }
          } else {
            writeReturn = this[_superWrite](import_buffer.Buffer.from(result));
          }
        }
        if (cb)
          cb();
        return writeReturn;
      }
    };
    Zlib = class extends ZlibBase {
      #level;
      #strategy;
      constructor(opts, mode) {
        opts = opts || {};
        opts.flush = opts.flush || constants.Z_NO_FLUSH;
        opts.finishFlush = opts.finishFlush || constants.Z_FINISH;
        opts.fullFlushFlag = constants.Z_FULL_FLUSH;
        super(opts, mode);
        this.#level = opts.level;
        this.#strategy = opts.strategy;
      }
      params(level, strategy) {
        if (this.sawError)
          return;
        if (!this.handle)
          throw new Error("cannot switch params when binding is closed");
        if (!this.handle.params)
          throw new Error("not supported in this implementation");
        if (this.#level !== level || this.#strategy !== strategy) {
          this.flush(constants.Z_SYNC_FLUSH);
          (0, import_assert2.default)(this.handle, "zlib binding closed");
          const origFlush = this.handle.flush;
          this.handle.flush = (flushFlag, cb) => {
            if (typeof flushFlag === "function") {
              cb = flushFlag;
              flushFlag = this.flushFlag;
            }
            this.flush(flushFlag);
            cb?.();
          };
          try {
            ;
            this.handle.params(level, strategy);
          } finally {
            this.handle.flush = origFlush;
          }
          if (this.handle) {
            this.#level = level;
            this.#strategy = strategy;
          }
        }
      }
    };
    Gzip = class extends Zlib {
      #portable;
      constructor(opts) {
        super(opts, "Gzip");
        this.#portable = opts && !!opts.portable;
      }
      [_superWrite](data) {
        if (!this.#portable)
          return super[_superWrite](data);
        this.#portable = false;
        data[9] = 255;
        return super[_superWrite](data);
      }
    };
    Unzip = class extends Zlib {
      constructor(opts) {
        super(opts, "Unzip");
      }
    };
    Brotli = class extends ZlibBase {
      constructor(opts, mode) {
        opts = opts || {};
        opts.flush = opts.flush || constants.BROTLI_OPERATION_PROCESS;
        opts.finishFlush = opts.finishFlush || constants.BROTLI_OPERATION_FINISH;
        opts.fullFlushFlag = constants.BROTLI_OPERATION_FLUSH;
        super(opts, mode);
      }
    };
    BrotliCompress = class extends Brotli {
      constructor(opts) {
        super(opts, "BrotliCompress");
      }
    };
    BrotliDecompress = class extends Brotli {
      constructor(opts) {
        super(opts, "BrotliDecompress");
      }
    };
    Zstd = class extends ZlibBase {
      constructor(opts, mode) {
        opts = opts || {};
        opts.flush = opts.flush || constants.ZSTD_e_continue;
        opts.finishFlush = opts.finishFlush || constants.ZSTD_e_end;
        opts.fullFlushFlag = constants.ZSTD_e_flush;
        super(opts, mode);
      }
    };
    ZstdCompress = class extends Zstd {
      constructor(opts) {
        super(opts, "ZstdCompress");
      }
    };
    ZstdDecompress = class extends Zstd {
      constructor(opts) {
        super(opts, "ZstdDecompress");
      }
    };
  }
});

// node_modules/tar/dist/esm/large-numbers.js
var encode, encodePositive, encodeNegative, parse, twos, pos, onesComp, twosComp;
var init_large_numbers = __esm({
  "node_modules/tar/dist/esm/large-numbers.js"() {
    encode = (num, buf) => {
      if (!Number.isSafeInteger(num)) {
        throw Error("cannot encode number outside of javascript safe integer range");
      } else if (num < 0) {
        encodeNegative(num, buf);
      } else {
        encodePositive(num, buf);
      }
      return buf;
    };
    encodePositive = (num, buf) => {
      buf[0] = 128;
      for (var i = buf.length; i > 1; i--) {
        buf[i - 1] = num & 255;
        num = Math.floor(num / 256);
      }
    };
    encodeNegative = (num, buf) => {
      buf[0] = 255;
      var flipped = false;
      num = num * -1;
      for (var i = buf.length; i > 1; i--) {
        var byte = num & 255;
        num = Math.floor(num / 256);
        if (flipped) {
          buf[i - 1] = onesComp(byte);
        } else if (byte === 0) {
          buf[i - 1] = 0;
        } else {
          flipped = true;
          buf[i - 1] = twosComp(byte);
        }
      }
    };
    parse = (buf) => {
      const pre = buf[0];
      const value = pre === 128 ? pos(buf.subarray(1, buf.length)) : pre === 255 ? twos(buf) : null;
      if (value === null) {
        throw Error("invalid base256 encoding");
      }
      if (!Number.isSafeInteger(value)) {
        throw Error("parsed number outside of javascript safe integer range");
      }
      return value;
    };
    twos = (buf) => {
      var len = buf.length;
      var sum = 0;
      var flipped = false;
      for (var i = len - 1; i > -1; i--) {
        var byte = Number(buf[i]);
        var f;
        if (flipped) {
          f = onesComp(byte);
        } else if (byte === 0) {
          f = byte;
        } else {
          flipped = true;
          f = twosComp(byte);
        }
        if (f !== 0) {
          sum -= f * Math.pow(256, len - i - 1);
        }
      }
      return sum;
    };
    pos = (buf) => {
      var len = buf.length;
      var sum = 0;
      for (var i = len - 1; i > -1; i--) {
        var byte = Number(buf[i]);
        if (byte !== 0) {
          sum += byte * Math.pow(256, len - i - 1);
        }
      }
      return sum;
    };
    onesComp = (byte) => (255 ^ byte) & 255;
    twosComp = (byte) => (255 ^ byte) + 1 & 255;
  }
});

// node_modules/tar/dist/esm/types.js
var isCode, name, code;
var init_types = __esm({
  "node_modules/tar/dist/esm/types.js"() {
    isCode = (c) => name.has(c);
    name = /* @__PURE__ */ new Map([
      ["0", "File"],
      // same as File
      ["", "OldFile"],
      ["1", "Link"],
      ["2", "SymbolicLink"],
      // Devices and FIFOs aren't fully supported
      // they are parsed, but skipped when unpacking
      ["3", "CharacterDevice"],
      ["4", "BlockDevice"],
      ["5", "Directory"],
      ["6", "FIFO"],
      // same as File
      ["7", "ContiguousFile"],
      // pax headers
      ["g", "GlobalExtendedHeader"],
      ["x", "ExtendedHeader"],
      // vendor-specific stuff
      // skip
      ["A", "SolarisACL"],
      // like 5, but with data, which should be skipped
      ["D", "GNUDumpDir"],
      // metadata only, skip
      ["I", "Inode"],
      // data = link path of next file
      ["K", "NextFileHasLongLinkpath"],
      // data = path of next file
      ["L", "NextFileHasLongPath"],
      // skip
      ["M", "ContinuationFile"],
      // like L
      ["N", "OldGnuLongPath"],
      // skip
      ["S", "SparseFile"],
      // skip
      ["V", "TapeVolumeHeader"],
      // like x
      ["X", "OldExtendedHeader"]
    ]);
    code = new Map(Array.from(name).map((kv) => [kv[1], kv[0]]));
  }
});

// node_modules/tar/dist/esm/header.js
var import_node_path, Header, splitPrefix, decString, decDate, numToDate, decNumber, nanUndef, decSmallNumber, MAXNUM, encNumber, encSmallNumber, octalString, padOctal, encDate, NULLS, encString;
var init_header = __esm({
  "node_modules/tar/dist/esm/header.js"() {
    import_node_path = require("node:path");
    init_large_numbers();
    init_types();
    Header = class {
      cksumValid = false;
      needPax = false;
      nullBlock = false;
      block;
      path;
      mode;
      uid;
      gid;
      size;
      cksum;
      #type = "Unsupported";
      linkpath;
      uname;
      gname;
      devmaj = 0;
      devmin = 0;
      atime;
      ctime;
      mtime;
      charset;
      comment;
      constructor(data, off = 0, ex, gex) {
        if (Buffer.isBuffer(data)) {
          this.decode(data, off || 0, ex, gex);
        } else if (data) {
          this.#slurp(data);
        }
      }
      decode(buf, off, ex, gex) {
        if (!off) {
          off = 0;
        }
        if (!buf || !(buf.length >= off + 512)) {
          throw new Error("need 512 bytes for header");
        }
        this.path = ex?.path ?? decString(buf, off, 100);
        this.mode = ex?.mode ?? gex?.mode ?? decNumber(buf, off + 100, 8);
        this.uid = ex?.uid ?? gex?.uid ?? decNumber(buf, off + 108, 8);
        this.gid = ex?.gid ?? gex?.gid ?? decNumber(buf, off + 116, 8);
        this.size = ex?.size ?? gex?.size ?? decNumber(buf, off + 124, 12);
        this.mtime = ex?.mtime ?? gex?.mtime ?? decDate(buf, off + 136, 12);
        this.cksum = decNumber(buf, off + 148, 12);
        if (gex)
          this.#slurp(gex, true);
        if (ex)
          this.#slurp(ex);
        const t = decString(buf, off + 156, 1);
        if (isCode(t)) {
          this.#type = t || "0";
        }
        if (this.#type === "0" && this.path.slice(-1) === "/") {
          this.#type = "5";
        }
        if (this.#type === "5") {
          this.size = 0;
        }
        this.linkpath = decString(buf, off + 157, 100);
        if (buf.subarray(off + 257, off + 265).toString() === "ustar\x0000") {
          this.uname = ex?.uname ?? gex?.uname ?? decString(buf, off + 265, 32);
          this.gname = ex?.gname ?? gex?.gname ?? decString(buf, off + 297, 32);
          this.devmaj = ex?.devmaj ?? gex?.devmaj ?? decNumber(buf, off + 329, 8) ?? 0;
          this.devmin = ex?.devmin ?? gex?.devmin ?? decNumber(buf, off + 337, 8) ?? 0;
          if (buf[off + 475] !== 0) {
            const prefix = decString(buf, off + 345, 155);
            this.path = prefix + "/" + this.path;
          } else {
            const prefix = decString(buf, off + 345, 130);
            if (prefix) {
              this.path = prefix + "/" + this.path;
            }
            this.atime = ex?.atime ?? gex?.atime ?? decDate(buf, off + 476, 12);
            this.ctime = ex?.ctime ?? gex?.ctime ?? decDate(buf, off + 488, 12);
          }
        }
        let sum = 8 * 32;
        for (let i = off; i < off + 148; i++) {
          sum += buf[i];
        }
        for (let i = off + 156; i < off + 512; i++) {
          sum += buf[i];
        }
        this.cksumValid = sum === this.cksum;
        if (this.cksum === void 0 && sum === 8 * 32) {
          this.nullBlock = true;
        }
      }
      #slurp(ex, gex = false) {
        Object.assign(this, Object.fromEntries(Object.entries(ex).filter(([k, v]) => {
          return !(v === null || v === void 0 || k === "path" && gex || k === "linkpath" && gex || k === "global");
        })));
      }
      encode(buf, off = 0) {
        if (!buf) {
          buf = this.block = Buffer.alloc(512);
        }
        if (this.#type === "Unsupported") {
          this.#type = "0";
        }
        if (!(buf.length >= off + 512)) {
          throw new Error("need 512 bytes for header");
        }
        const prefixSize = this.ctime || this.atime ? 130 : 155;
        const split = splitPrefix(this.path || "", prefixSize);
        const path16 = split[0];
        const prefix = split[1];
        this.needPax = !!split[2];
        this.needPax = encString(buf, off, 100, path16) || this.needPax;
        this.needPax = encNumber(buf, off + 100, 8, this.mode) || this.needPax;
        this.needPax = encNumber(buf, off + 108, 8, this.uid) || this.needPax;
        this.needPax = encNumber(buf, off + 116, 8, this.gid) || this.needPax;
        this.needPax = encNumber(buf, off + 124, 12, this.size) || this.needPax;
        this.needPax = encDate(buf, off + 136, 12, this.mtime) || this.needPax;
        buf[off + 156] = Number(this.#type.codePointAt(0));
        this.needPax = encString(buf, off + 157, 100, this.linkpath) || this.needPax;
        buf.write("ustar\x0000", off + 257, 8);
        this.needPax = encString(buf, off + 265, 32, this.uname) || this.needPax;
        this.needPax = encString(buf, off + 297, 32, this.gname) || this.needPax;
        this.needPax = encNumber(buf, off + 329, 8, this.devmaj) || this.needPax;
        this.needPax = encNumber(buf, off + 337, 8, this.devmin) || this.needPax;
        this.needPax = encString(buf, off + 345, prefixSize, prefix) || this.needPax;
        if (buf[off + 475] !== 0) {
          this.needPax = encString(buf, off + 345, 155, prefix) || this.needPax;
        } else {
          this.needPax = encString(buf, off + 345, 130, prefix) || this.needPax;
          this.needPax = encDate(buf, off + 476, 12, this.atime) || this.needPax;
          this.needPax = encDate(buf, off + 488, 12, this.ctime) || this.needPax;
        }
        let sum = 8 * 32;
        for (let i = off; i < off + 148; i++) {
          sum += buf[i];
        }
        for (let i = off + 156; i < off + 512; i++) {
          sum += buf[i];
        }
        this.cksum = sum;
        encNumber(buf, off + 148, 8, this.cksum);
        this.cksumValid = true;
        return this.needPax;
      }
      get type() {
        return this.#type === "Unsupported" ? this.#type : name.get(this.#type);
      }
      get typeKey() {
        return this.#type;
      }
      set type(type) {
        const c = String(code.get(type));
        if (isCode(c) || c === "Unsupported") {
          this.#type = c;
        } else if (isCode(type)) {
          this.#type = type;
        } else {
          throw new TypeError("invalid entry type: " + type);
        }
      }
    };
    splitPrefix = (p, prefixSize) => {
      const pathSize = 100;
      let pp = p;
      let prefix = "";
      let ret = void 0;
      const root = import_node_path.posix.parse(p).root || ".";
      if (Buffer.byteLength(pp) < pathSize) {
        ret = [pp, prefix, false];
      } else {
        prefix = import_node_path.posix.dirname(pp);
        pp = import_node_path.posix.basename(pp);
        do {
          if (Buffer.byteLength(pp) <= pathSize && Buffer.byteLength(prefix) <= prefixSize) {
            ret = [pp, prefix, false];
          } else if (Buffer.byteLength(pp) > pathSize && Buffer.byteLength(prefix) <= prefixSize) {
            ret = [pp.slice(0, pathSize - 1), prefix, true];
          } else {
            pp = import_node_path.posix.join(import_node_path.posix.basename(prefix), pp);
            prefix = import_node_path.posix.dirname(prefix);
          }
        } while (prefix !== root && ret === void 0);
        if (!ret) {
          ret = [p.slice(0, pathSize - 1), "", true];
        }
      }
      return ret;
    };
    decString = (buf, off, size) => buf.subarray(off, off + size).toString("utf8").replace(/\0.*/, "");
    decDate = (buf, off, size) => numToDate(decNumber(buf, off, size));
    numToDate = (num) => num === void 0 ? void 0 : new Date(num * 1e3);
    decNumber = (buf, off, size) => Number(buf[off]) & 128 ? parse(buf.subarray(off, off + size)) : decSmallNumber(buf, off, size);
    nanUndef = (value) => isNaN(value) ? void 0 : value;
    decSmallNumber = (buf, off, size) => nanUndef(parseInt(buf.subarray(off, off + size).toString("utf8").replace(/\0.*$/, "").trim(), 8));
    MAXNUM = {
      12: 8589934591,
      8: 2097151
    };
    encNumber = (buf, off, size, num) => num === void 0 ? false : num > MAXNUM[size] || num < 0 ? (encode(num, buf.subarray(off, off + size)), true) : (encSmallNumber(buf, off, size, num), false);
    encSmallNumber = (buf, off, size, num) => buf.write(octalString(num, size), off, size, "ascii");
    octalString = (num, size) => padOctal(Math.floor(num).toString(8), size);
    padOctal = (str, size) => (str.length === size - 1 ? str : new Array(size - str.length - 1).join("0") + str + " ") + "\0";
    encDate = (buf, off, size, date) => date === void 0 ? false : encNumber(buf, off, size, date.getTime() / 1e3);
    NULLS = new Array(156).join("\0");
    encString = (buf, off, size, str) => str === void 0 ? false : (buf.write(str + NULLS, off, size, "utf8"), str.length !== Buffer.byteLength(str) || str.length > size);
  }
});

// node_modules/tar/dist/esm/pax.js
var import_node_path2, Pax, merge, parseKV, parseKVLine;
var init_pax = __esm({
  "node_modules/tar/dist/esm/pax.js"() {
    import_node_path2 = require("node:path");
    init_header();
    Pax = class _Pax {
      atime;
      mtime;
      ctime;
      charset;
      comment;
      gid;
      uid;
      gname;
      uname;
      linkpath;
      dev;
      ino;
      nlink;
      path;
      size;
      mode;
      global;
      constructor(obj, global2 = false) {
        this.atime = obj.atime;
        this.charset = obj.charset;
        this.comment = obj.comment;
        this.ctime = obj.ctime;
        this.dev = obj.dev;
        this.gid = obj.gid;
        this.global = global2;
        this.gname = obj.gname;
        this.ino = obj.ino;
        this.linkpath = obj.linkpath;
        this.mtime = obj.mtime;
        this.nlink = obj.nlink;
        this.path = obj.path;
        this.size = obj.size;
        this.uid = obj.uid;
        this.uname = obj.uname;
      }
      encode() {
        const body = this.encodeBody();
        if (body === "") {
          return Buffer.allocUnsafe(0);
        }
        const bodyLen = Buffer.byteLength(body);
        const bufLen = 512 * Math.ceil(1 + bodyLen / 512);
        const buf = Buffer.allocUnsafe(bufLen);
        for (let i = 0; i < 512; i++) {
          buf[i] = 0;
        }
        new Header({
          // XXX split the path
          // then the path should be PaxHeader + basename, but less than 99,
          // prepend with the dirname
          /* c8 ignore start */
          path: ("PaxHeader/" + (0, import_node_path2.basename)(this.path ?? "")).slice(0, 99),
          /* c8 ignore stop */
          mode: this.mode || 420,
          uid: this.uid,
          gid: this.gid,
          size: bodyLen,
          mtime: this.mtime,
          type: this.global ? "GlobalExtendedHeader" : "ExtendedHeader",
          linkpath: "",
          uname: this.uname || "",
          gname: this.gname || "",
          devmaj: 0,
          devmin: 0,
          atime: this.atime,
          ctime: this.ctime
        }).encode(buf);
        buf.write(body, 512, bodyLen, "utf8");
        for (let i = bodyLen + 512; i < buf.length; i++) {
          buf[i] = 0;
        }
        return buf;
      }
      encodeBody() {
        return this.encodeField("path") + this.encodeField("ctime") + this.encodeField("atime") + this.encodeField("dev") + this.encodeField("ino") + this.encodeField("nlink") + this.encodeField("charset") + this.encodeField("comment") + this.encodeField("gid") + this.encodeField("gname") + this.encodeField("linkpath") + this.encodeField("mtime") + this.encodeField("size") + this.encodeField("uid") + this.encodeField("uname");
      }
      encodeField(field) {
        if (this[field] === void 0) {
          return "";
        }
        const r = this[field];
        const v = r instanceof Date ? r.getTime() / 1e3 : r;
        const s = " " + (field === "dev" || field === "ino" || field === "nlink" ? "SCHILY." : "") + field + "=" + v + "\n";
        const byteLen = Buffer.byteLength(s);
        let digits = Math.floor(Math.log(byteLen) / Math.log(10)) + 1;
        if (byteLen + digits >= Math.pow(10, digits)) {
          digits += 1;
        }
        const len = digits + byteLen;
        return len + s;
      }
      static parse(str, ex, g = false) {
        return new _Pax(merge(parseKV(str), ex), g);
      }
    };
    merge = (a, b) => b ? Object.assign({}, b, a) : a;
    parseKV = (str) => str.replace(/\n$/, "").split("\n").reduce(parseKVLine, /* @__PURE__ */ Object.create(null));
    parseKVLine = (set, line) => {
      const n = parseInt(line, 10);
      if (n !== Buffer.byteLength(line) + 1) {
        return set;
      }
      line = line.slice((n + " ").length);
      const kv = line.split("=");
      const r = kv.shift();
      if (!r) {
        return set;
      }
      const k = r.replace(/^SCHILY\.(dev|ino|nlink)/, "$1");
      const v = kv.join("=");
      set[k] = /^([A-Z]+\.)?([mac]|birth|creation)time$/.test(k) ? new Date(Number(v) * 1e3) : /^[0-9]+$/.test(v) ? +v : v;
      return set;
    };
  }
});

// node_modules/tar/dist/esm/normalize-windows-path.js
var platform, normalizeWindowsPath;
var init_normalize_windows_path = __esm({
  "node_modules/tar/dist/esm/normalize-windows-path.js"() {
    platform = process.env.TESTING_TAR_FAKE_PLATFORM || process.platform;
    normalizeWindowsPath = platform !== "win32" ? (p) => p : (p) => p && p.replaceAll(/\\/g, "/");
  }
});

// node_modules/tar/dist/esm/read-entry.js
var ReadEntry;
var init_read_entry = __esm({
  "node_modules/tar/dist/esm/read-entry.js"() {
    init_esm();
    init_normalize_windows_path();
    ReadEntry = class extends Minipass {
      extended;
      globalExtended;
      header;
      startBlockSize;
      blockRemain;
      remain;
      type;
      meta = false;
      ignore = false;
      path;
      mode;
      uid;
      gid;
      uname;
      gname;
      size = 0;
      mtime;
      atime;
      ctime;
      linkpath;
      dev;
      ino;
      nlink;
      invalid = false;
      absolute;
      unsupported = false;
      constructor(header, ex, gex) {
        super({});
        this.pause();
        this.extended = ex;
        this.globalExtended = gex;
        this.header = header;
        this.remain = header.size ?? 0;
        this.startBlockSize = 512 * Math.ceil(this.remain / 512);
        this.blockRemain = this.startBlockSize;
        this.type = header.type;
        switch (this.type) {
          case "File":
          case "OldFile":
          case "Link":
          case "SymbolicLink":
          case "CharacterDevice":
          case "BlockDevice":
          case "Directory":
          case "FIFO":
          case "ContiguousFile":
          case "GNUDumpDir":
            break;
          case "NextFileHasLongLinkpath":
          case "NextFileHasLongPath":
          case "OldGnuLongPath":
          case "GlobalExtendedHeader":
          case "ExtendedHeader":
          case "OldExtendedHeader":
            this.meta = true;
            break;
          // NOTE: gnutar and bsdtar treat unrecognized types as 'File'
          // it may be worth doing the same, but with a warning.
          default:
            this.ignore = true;
        }
        if (!header.path) {
          throw new Error("no path provided for tar.ReadEntry");
        }
        this.path = normalizeWindowsPath(header.path);
        this.mode = header.mode;
        if (this.mode) {
          this.mode = this.mode & 4095;
        }
        this.uid = header.uid;
        this.gid = header.gid;
        this.uname = header.uname;
        this.gname = header.gname;
        this.size = this.remain;
        this.mtime = header.mtime;
        this.atime = header.atime;
        this.ctime = header.ctime;
        this.linkpath = header.linkpath ? normalizeWindowsPath(header.linkpath) : void 0;
        this.uname = header.uname;
        this.gname = header.gname;
        if (ex) {
          this.#slurp(ex);
        }
        if (gex) {
          this.#slurp(gex, true);
        }
      }
      write(data) {
        const writeLen = data.length;
        if (writeLen > this.blockRemain) {
          throw new Error("writing more to entry than is appropriate");
        }
        const r = this.remain;
        const br = this.blockRemain;
        this.remain = Math.max(0, r - writeLen);
        this.blockRemain = Math.max(0, br - writeLen);
        if (this.ignore) {
          return true;
        }
        if (r >= writeLen) {
          return super.write(data);
        }
        return super.write(data.subarray(0, r));
      }
      #slurp(ex, gex = false) {
        if (ex.path)
          ex.path = normalizeWindowsPath(ex.path);
        if (ex.linkpath)
          ex.linkpath = normalizeWindowsPath(ex.linkpath);
        Object.assign(this, Object.fromEntries(Object.entries(ex).filter(([k, v]) => {
          return !(v === null || v === void 0 || k === "path" && gex);
        })));
      }
    };
  }
});

// node_modules/tar/dist/esm/warn-method.js
var warnMethod;
var init_warn_method = __esm({
  "node_modules/tar/dist/esm/warn-method.js"() {
    warnMethod = (self2, code2, message, data = {}) => {
      if (self2.file) {
        data.file = self2.file;
      }
      if (self2.cwd) {
        data.cwd = self2.cwd;
      }
      data.code = message instanceof Error && message.code || code2;
      data.tarCode = code2;
      if (!self2.strict && data.recoverable !== false) {
        if (message instanceof Error) {
          data = Object.assign(message, data);
          message = message.message;
        }
        self2.emit("warn", code2, message, data);
      } else if (message instanceof Error) {
        self2.emit("error", Object.assign(message, data));
      } else {
        self2.emit("error", Object.assign(new Error(`${code2}: ${message}`), data));
      }
    };
  }
});

// node_modules/tar/dist/esm/parse.js
var import_events3, maxMetaEntrySize, gzipHeader, zstdHeader, ZIP_HEADER_LEN, STATE, WRITEENTRY, READENTRY, NEXTENTRY, PROCESSENTRY, EX, GEX, META, EMITMETA, BUFFER2, QUEUE, ENDED, EMITTEDEND, EMIT, UNZIP, CONSUMECHUNK, CONSUMECHUNKSUB, CONSUMEBODY, CONSUMEMETA, CONSUMEHEADER, CONSUMING, BUFFERCONCAT, MAYBEEND, WRITING, ABORTED2, DONE, SAW_VALID_ENTRY, SAW_NULL_BLOCK, SAW_EOF, CLOSESTREAM, noop2, Parser;
var init_parse = __esm({
  "node_modules/tar/dist/esm/parse.js"() {
    import_events3 = require("events");
    init_esm3();
    init_header();
    init_pax();
    init_read_entry();
    init_warn_method();
    maxMetaEntrySize = 1024 * 1024;
    gzipHeader = Buffer.from([31, 139]);
    zstdHeader = Buffer.from([40, 181, 47, 253]);
    ZIP_HEADER_LEN = Math.max(gzipHeader.length, zstdHeader.length);
    STATE = /* @__PURE__ */ Symbol("state");
    WRITEENTRY = /* @__PURE__ */ Symbol("writeEntry");
    READENTRY = /* @__PURE__ */ Symbol("readEntry");
    NEXTENTRY = /* @__PURE__ */ Symbol("nextEntry");
    PROCESSENTRY = /* @__PURE__ */ Symbol("processEntry");
    EX = /* @__PURE__ */ Symbol("extendedHeader");
    GEX = /* @__PURE__ */ Symbol("globalExtendedHeader");
    META = /* @__PURE__ */ Symbol("meta");
    EMITMETA = /* @__PURE__ */ Symbol("emitMeta");
    BUFFER2 = /* @__PURE__ */ Symbol("buffer");
    QUEUE = /* @__PURE__ */ Symbol("queue");
    ENDED = /* @__PURE__ */ Symbol("ended");
    EMITTEDEND = /* @__PURE__ */ Symbol("emittedEnd");
    EMIT = /* @__PURE__ */ Symbol("emit");
    UNZIP = /* @__PURE__ */ Symbol("unzip");
    CONSUMECHUNK = /* @__PURE__ */ Symbol("consumeChunk");
    CONSUMECHUNKSUB = /* @__PURE__ */ Symbol("consumeChunkSub");
    CONSUMEBODY = /* @__PURE__ */ Symbol("consumeBody");
    CONSUMEMETA = /* @__PURE__ */ Symbol("consumeMeta");
    CONSUMEHEADER = /* @__PURE__ */ Symbol("consumeHeader");
    CONSUMING = /* @__PURE__ */ Symbol("consuming");
    BUFFERCONCAT = /* @__PURE__ */ Symbol("bufferConcat");
    MAYBEEND = /* @__PURE__ */ Symbol("maybeEnd");
    WRITING = /* @__PURE__ */ Symbol("writing");
    ABORTED2 = /* @__PURE__ */ Symbol("aborted");
    DONE = /* @__PURE__ */ Symbol("onDone");
    SAW_VALID_ENTRY = /* @__PURE__ */ Symbol("sawValidEntry");
    SAW_NULL_BLOCK = /* @__PURE__ */ Symbol("sawNullBlock");
    SAW_EOF = /* @__PURE__ */ Symbol("sawEOF");
    CLOSESTREAM = /* @__PURE__ */ Symbol("closeStream");
    noop2 = () => true;
    Parser = class extends import_events3.EventEmitter {
      file;
      strict;
      maxMetaEntrySize;
      filter;
      brotli;
      zstd;
      writable = true;
      readable = false;
      [QUEUE] = [];
      [BUFFER2];
      [READENTRY];
      [WRITEENTRY];
      [STATE] = "begin";
      [META] = "";
      [EX];
      [GEX];
      [ENDED] = false;
      [UNZIP];
      [ABORTED2] = false;
      [SAW_VALID_ENTRY];
      [SAW_NULL_BLOCK] = false;
      [SAW_EOF] = false;
      [WRITING] = false;
      [CONSUMING] = false;
      [EMITTEDEND] = false;
      constructor(opt = {}) {
        super();
        this.file = opt.file || "";
        this.on(DONE, () => {
          if (this[STATE] === "begin" || this[SAW_VALID_ENTRY] === false) {
            this.warn("TAR_BAD_ARCHIVE", "Unrecognized archive format");
          }
        });
        if (opt.ondone) {
          this.on(DONE, opt.ondone);
        } else {
          this.on(DONE, () => {
            this.emit("prefinish");
            this.emit("finish");
            this.emit("end");
          });
        }
        this.strict = !!opt.strict;
        this.maxMetaEntrySize = opt.maxMetaEntrySize || maxMetaEntrySize;
        this.filter = typeof opt.filter === "function" ? opt.filter : noop2;
        const isTBR = opt.file && (opt.file.endsWith(".tar.br") || opt.file.endsWith(".tbr"));
        this.brotli = !(opt.gzip || opt.zstd) && opt.brotli !== void 0 ? opt.brotli : isTBR ? void 0 : false;
        const isTZST = opt.file && (opt.file.endsWith(".tar.zst") || opt.file.endsWith(".tzst"));
        this.zstd = !(opt.gzip || opt.brotli) && opt.zstd !== void 0 ? opt.zstd : isTZST ? true : void 0;
        this.on("end", () => this[CLOSESTREAM]());
        if (typeof opt.onwarn === "function") {
          this.on("warn", opt.onwarn);
        }
        if (typeof opt.onReadEntry === "function") {
          this.on("entry", opt.onReadEntry);
        }
      }
      warn(code2, message, data = {}) {
        warnMethod(this, code2, message, data);
      }
      [CONSUMEHEADER](chunk, position) {
        if (this[SAW_VALID_ENTRY] === void 0) {
          this[SAW_VALID_ENTRY] = false;
        }
        let header;
        try {
          header = new Header(chunk, position, this[EX], this[GEX]);
        } catch (er) {
          return this.warn("TAR_ENTRY_INVALID", er);
        }
        if (header.nullBlock) {
          if (this[SAW_NULL_BLOCK]) {
            this[SAW_EOF] = true;
            if (this[STATE] === "begin") {
              this[STATE] = "header";
            }
            this[EMIT]("eof");
          } else {
            this[SAW_NULL_BLOCK] = true;
            this[EMIT]("nullBlock");
          }
        } else {
          this[SAW_NULL_BLOCK] = false;
          if (!header.cksumValid) {
            this.warn("TAR_ENTRY_INVALID", "checksum failure", { header });
          } else if (!header.path) {
            this.warn("TAR_ENTRY_INVALID", "path is required", { header });
          } else {
            const type = header.type;
            if (/^(Symbolic)?Link$/.test(type) && !header.linkpath) {
              this.warn("TAR_ENTRY_INVALID", "linkpath required", {
                header
              });
            } else if (!/^(Symbolic)?Link$/.test(type) && !/^(Global)?ExtendedHeader$/.test(type) && header.linkpath) {
              this.warn("TAR_ENTRY_INVALID", "linkpath forbidden", {
                header
              });
            } else {
              const entry = this[WRITEENTRY] = new ReadEntry(header, this[EX], this[GEX]);
              if (!this[SAW_VALID_ENTRY]) {
                if (entry.remain) {
                  const onend = () => {
                    if (!entry.invalid) {
                      this[SAW_VALID_ENTRY] = true;
                    }
                  };
                  entry.on("end", onend);
                } else {
                  this[SAW_VALID_ENTRY] = true;
                }
              }
              if (entry.meta) {
                if (entry.size > this.maxMetaEntrySize) {
                  entry.ignore = true;
                  this[EMIT]("ignoredEntry", entry);
                  this[STATE] = "ignore";
                  entry.resume();
                } else if (entry.size > 0) {
                  this[META] = "";
                  entry.on("data", (c) => this[META] += c);
                  this[STATE] = "meta";
                }
              } else {
                this[EX] = void 0;
                entry.ignore = entry.ignore || !this.filter(entry.path, entry);
                if (entry.ignore) {
                  this[EMIT]("ignoredEntry", entry);
                  this[STATE] = entry.remain ? "ignore" : "header";
                  entry.resume();
                } else {
                  if (entry.remain) {
                    this[STATE] = "body";
                  } else {
                    this[STATE] = "header";
                    entry.end();
                  }
                  if (!this[READENTRY]) {
                    this[QUEUE].push(entry);
                    this[NEXTENTRY]();
                  } else {
                    this[QUEUE].push(entry);
                  }
                }
              }
            }
          }
        }
      }
      [CLOSESTREAM]() {
        queueMicrotask(() => this.emit("close"));
      }
      [PROCESSENTRY](entry) {
        let go = true;
        if (!entry) {
          this[READENTRY] = void 0;
          go = false;
        } else if (Array.isArray(entry)) {
          const [ev, ...args] = entry;
          this.emit(ev, ...args);
        } else {
          this[READENTRY] = entry;
          this.emit("entry", entry);
          if (!entry.emittedEnd) {
            entry.on("end", () => this[NEXTENTRY]());
            go = false;
          }
        }
        return go;
      }
      [NEXTENTRY]() {
        do {
        } while (this[PROCESSENTRY](this[QUEUE].shift()));
        if (this[QUEUE].length === 0) {
          const re = this[READENTRY];
          const drainNow = !re || re.flowing || re.size === re.remain;
          if (drainNow) {
            if (!this[WRITING]) {
              this.emit("drain");
            }
          } else {
            re.once("drain", () => this.emit("drain"));
          }
        }
      }
      [CONSUMEBODY](chunk, position) {
        const entry = this[WRITEENTRY];
        if (!entry) {
          throw new Error("attempt to consume body without entry??");
        }
        const br = entry.blockRemain ?? 0;
        const c = br >= chunk.length && position === 0 ? chunk : chunk.subarray(position, position + br);
        entry.write(c);
        if (!entry.blockRemain) {
          this[STATE] = "header";
          this[WRITEENTRY] = void 0;
          entry.end();
        }
        return c.length;
      }
      [CONSUMEMETA](chunk, position) {
        const entry = this[WRITEENTRY];
        const ret = this[CONSUMEBODY](chunk, position);
        if (!this[WRITEENTRY] && entry) {
          this[EMITMETA](entry);
        }
        return ret;
      }
      [EMIT](ev, data, extra) {
        if (this[QUEUE].length === 0 && !this[READENTRY]) {
          this.emit(ev, data, extra);
        } else {
          this[QUEUE].push([ev, data, extra]);
        }
      }
      [EMITMETA](entry) {
        this[EMIT]("meta", this[META]);
        switch (entry.type) {
          case "ExtendedHeader":
          case "OldExtendedHeader":
            this[EX] = Pax.parse(this[META], this[EX], false);
            break;
          case "GlobalExtendedHeader":
            this[GEX] = Pax.parse(this[META], this[GEX], true);
            break;
          case "NextFileHasLongPath":
          case "OldGnuLongPath": {
            const ex = this[EX] ?? /* @__PURE__ */ Object.create(null);
            this[EX] = ex;
            ex.path = this[META].replace(/\0.*/, "");
            break;
          }
          case "NextFileHasLongLinkpath": {
            const ex = this[EX] || /* @__PURE__ */ Object.create(null);
            this[EX] = ex;
            ex.linkpath = this[META].replace(/\0.*/, "");
            break;
          }
          /* c8 ignore start */
          default:
            throw new Error("unknown meta: " + entry.type);
        }
      }
      abort(error) {
        this[ABORTED2] = true;
        this.emit("abort", error);
        this.warn("TAR_ABORT", error, { recoverable: false });
      }
      write(chunk, encoding, cb) {
        if (typeof encoding === "function") {
          cb = encoding;
          encoding = void 0;
        }
        if (typeof chunk === "string") {
          chunk = Buffer.from(
            chunk,
            /* c8 ignore next */
            typeof encoding === "string" ? encoding : "utf8"
          );
        }
        if (this[ABORTED2]) {
          cb?.();
          return false;
        }
        const needSniff = this[UNZIP] === void 0 || this.brotli === void 0 && this[UNZIP] === false;
        if (needSniff && chunk) {
          if (this[BUFFER2]) {
            chunk = Buffer.concat([this[BUFFER2], chunk]);
            this[BUFFER2] = void 0;
          }
          if (chunk.length < ZIP_HEADER_LEN) {
            this[BUFFER2] = chunk;
            cb?.();
            return true;
          }
          for (let i = 0; this[UNZIP] === void 0 && i < gzipHeader.length; i++) {
            if (chunk[i] !== gzipHeader[i]) {
              this[UNZIP] = false;
            }
          }
          let isZstd = false;
          if (this[UNZIP] === false && this.zstd !== false) {
            isZstd = true;
            for (let i = 0; i < zstdHeader.length; i++) {
              if (chunk[i] !== zstdHeader[i]) {
                isZstd = false;
                break;
              }
            }
          }
          const maybeBrotli = this.brotli === void 0 && !isZstd;
          if (this[UNZIP] === false && maybeBrotli) {
            if (chunk.length < 512) {
              if (this[ENDED]) {
                this.brotli = true;
              } else {
                this[BUFFER2] = chunk;
                cb?.();
                return true;
              }
            } else {
              try {
                new Header(chunk.subarray(0, 512));
                this.brotli = false;
              } catch (_) {
                this.brotli = true;
              }
            }
          }
          if (this[UNZIP] === void 0 || this[UNZIP] === false && (this.brotli || isZstd)) {
            const ended = this[ENDED];
            this[ENDED] = false;
            this[UNZIP] = this[UNZIP] === void 0 ? new Unzip({}) : isZstd ? new ZstdDecompress({}) : new BrotliDecompress({});
            this[UNZIP].on("data", (chunk2) => this[CONSUMECHUNK](chunk2));
            this[UNZIP].on("error", (er) => this.abort(er));
            this[UNZIP].on("end", () => {
              this[ENDED] = true;
              this[CONSUMECHUNK]();
            });
            this[WRITING] = true;
            const ret2 = !!this[UNZIP][ended ? "end" : "write"](chunk);
            this[WRITING] = false;
            cb?.();
            return ret2;
          }
        }
        this[WRITING] = true;
        if (this[UNZIP]) {
          this[UNZIP].write(chunk);
        } else {
          this[CONSUMECHUNK](chunk);
        }
        this[WRITING] = false;
        const ret = this[QUEUE].length > 0 ? false : this[READENTRY] ? this[READENTRY].flowing : true;
        if (!ret && this[QUEUE].length === 0) {
          this[READENTRY]?.once("drain", () => this.emit("drain"));
        }
        cb?.();
        return ret;
      }
      [BUFFERCONCAT](c) {
        if (c && !this[ABORTED2]) {
          this[BUFFER2] = this[BUFFER2] ? Buffer.concat([this[BUFFER2], c]) : c;
        }
      }
      [MAYBEEND]() {
        if (this[ENDED] && !this[EMITTEDEND] && !this[ABORTED2] && !this[CONSUMING]) {
          this[EMITTEDEND] = true;
          const entry = this[WRITEENTRY];
          if (entry && entry.blockRemain) {
            const have = this[BUFFER2] ? this[BUFFER2].length : 0;
            this.warn("TAR_BAD_ARCHIVE", `Truncated input (needed ${entry.blockRemain} more bytes, only ${have} available)`, { entry });
            if (this[BUFFER2]) {
              entry.write(this[BUFFER2]);
            }
            entry.end();
          }
          this[EMIT](DONE);
        }
      }
      [CONSUMECHUNK](chunk) {
        if (this[CONSUMING] && chunk) {
          this[BUFFERCONCAT](chunk);
        } else if (!chunk && !this[BUFFER2]) {
          this[MAYBEEND]();
        } else if (chunk) {
          this[CONSUMING] = true;
          if (this[BUFFER2]) {
            this[BUFFERCONCAT](chunk);
            const c = this[BUFFER2];
            this[BUFFER2] = void 0;
            this[CONSUMECHUNKSUB](c);
          } else {
            this[CONSUMECHUNKSUB](chunk);
          }
          while (this[BUFFER2] && this[BUFFER2]?.length >= 512 && !this[ABORTED2] && !this[SAW_EOF]) {
            const c = this[BUFFER2];
            this[BUFFER2] = void 0;
            this[CONSUMECHUNKSUB](c);
          }
          this[CONSUMING] = false;
        }
        if (!this[BUFFER2] || this[ENDED]) {
          this[MAYBEEND]();
        }
      }
      [CONSUMECHUNKSUB](chunk) {
        let position = 0;
        const length = chunk.length;
        while (position + 512 <= length && !this[ABORTED2] && !this[SAW_EOF]) {
          switch (this[STATE]) {
            case "begin":
            case "header":
              this[CONSUMEHEADER](chunk, position);
              position += 512;
              break;
            case "ignore":
            case "body":
              position += this[CONSUMEBODY](chunk, position);
              break;
            case "meta":
              position += this[CONSUMEMETA](chunk, position);
              break;
            /* c8 ignore start */
            default:
              throw new Error("invalid state: " + this[STATE]);
          }
        }
        if (position < length) {
          this[BUFFER2] = this[BUFFER2] ? Buffer.concat([chunk.subarray(position), this[BUFFER2]]) : chunk.subarray(position);
        }
      }
      end(chunk, encoding, cb) {
        if (typeof chunk === "function") {
          cb = chunk;
          encoding = void 0;
          chunk = void 0;
        }
        if (typeof encoding === "function") {
          cb = encoding;
          encoding = void 0;
        }
        if (typeof chunk === "string") {
          chunk = Buffer.from(chunk, encoding);
        }
        if (cb)
          this.once("finish", cb);
        if (!this[ABORTED2]) {
          if (this[UNZIP]) {
            if (chunk)
              this[UNZIP].write(chunk);
            this[UNZIP].end();
          } else {
            this[ENDED] = true;
            if (this.brotli === void 0 || this.zstd === void 0)
              chunk = chunk || Buffer.alloc(0);
            if (chunk)
              this.write(chunk);
            this[MAYBEEND]();
          }
        }
        return this;
      }
    };
  }
});

// node_modules/tar/dist/esm/strip-trailing-slashes.js
var stripTrailingSlashes;
var init_strip_trailing_slashes = __esm({
  "node_modules/tar/dist/esm/strip-trailing-slashes.js"() {
    stripTrailingSlashes = (str) => {
      let i = str.length - 1;
      let slashesStart = -1;
      while (i > -1 && str.charAt(i) === "/") {
        slashesStart = i;
        i--;
      }
      return slashesStart === -1 ? str : str.slice(0, slashesStart);
    };
  }
});

// node_modules/tar/dist/esm/list.js
var list_exports = {};
__export(list_exports, {
  filesFilter: () => filesFilter,
  list: () => list
});
var import_node_fs, import_path2, onReadEntryFunction, filesFilter, listFileSync, listFile, list;
var init_list = __esm({
  "node_modules/tar/dist/esm/list.js"() {
    init_esm2();
    import_node_fs = __toESM(require("node:fs"), 1);
    import_path2 = require("path");
    init_make_command();
    init_parse();
    init_strip_trailing_slashes();
    onReadEntryFunction = (opt) => {
      const onReadEntry = opt.onReadEntry;
      opt.onReadEntry = onReadEntry ? (e) => {
        onReadEntry(e);
        e.resume();
      } : (e) => e.resume();
    };
    filesFilter = (opt, files) => {
      const map = new Map(files.map((f) => [stripTrailingSlashes(f), true]));
      const filter = opt.filter;
      const mapHas = (file, r = "") => {
        const root = r || (0, import_path2.parse)(file).root || ".";
        let ret;
        if (file === root)
          ret = false;
        else {
          const m = map.get(file);
          ret = m !== void 0 ? m : mapHas((0, import_path2.dirname)(file), root);
        }
        map.set(file, ret);
        return ret;
      };
      opt.filter = filter ? (file, entry) => filter(file, entry) && mapHas(stripTrailingSlashes(file)) : (file) => mapHas(stripTrailingSlashes(file));
    };
    listFileSync = (opt) => {
      const p = new Parser(opt);
      const file = opt.file;
      let fd;
      try {
        fd = import_node_fs.default.openSync(file, "r");
        const stat = import_node_fs.default.fstatSync(fd);
        const readSize = opt.maxReadSize || 16 * 1024 * 1024;
        if (stat.size < readSize) {
          const buf = Buffer.allocUnsafe(stat.size);
          const read = import_node_fs.default.readSync(fd, buf, 0, stat.size, 0);
          p.end(read === buf.byteLength ? buf : buf.subarray(0, read));
        } else {
          let pos2 = 0;
          const buf = Buffer.allocUnsafe(readSize);
          while (pos2 < stat.size) {
            const bytesRead = import_node_fs.default.readSync(fd, buf, 0, readSize, pos2);
            if (bytesRead === 0)
              break;
            pos2 += bytesRead;
            p.write(buf.subarray(0, bytesRead));
          }
          p.end();
        }
      } finally {
        if (typeof fd === "number") {
          try {
            import_node_fs.default.closeSync(fd);
          } catch {
          }
        }
      }
    };
    listFile = (opt, _files) => {
      const parse4 = new Parser(opt);
      const readSize = opt.maxReadSize || 16 * 1024 * 1024;
      const file = opt.file;
      const p = new Promise((resolve, reject) => {
        parse4.on("error", reject);
        parse4.on("end", resolve);
        import_node_fs.default.stat(file, (er, stat) => {
          if (er) {
            reject(er);
          } else {
            const stream = new ReadStream(file, {
              readSize,
              size: stat.size
            });
            stream.on("error", reject);
            stream.pipe(parse4);
          }
        });
      });
      return p;
    };
    list = makeCommand(listFileSync, listFile, (opt) => new Parser(opt), (opt) => new Parser(opt), (opt, files) => {
      if (files?.length)
        filesFilter(opt, files);
      if (!opt.noResume)
        onReadEntryFunction(opt);
    });
  }
});

// node_modules/tar/dist/esm/get-write-flag.js
var import_fs3, platform2, isWindows, O_CREAT, O_NOFOLLOW, O_TRUNC, O_WRONLY, UV_FS_O_FILEMAP, fMapEnabled, fMapLimit, fMapFlag, noFollowFlag, getWriteFlag;
var init_get_write_flag = __esm({
  "node_modules/tar/dist/esm/get-write-flag.js"() {
    import_fs3 = __toESM(require("fs"), 1);
    platform2 = process.env.__FAKE_PLATFORM__ || process.platform;
    isWindows = platform2 === "win32";
    ({ O_CREAT, O_NOFOLLOW, O_TRUNC, O_WRONLY } = import_fs3.default.constants);
    UV_FS_O_FILEMAP = Number(process.env.__FAKE_FS_O_FILENAME__) || import_fs3.default.constants.UV_FS_O_FILEMAP || 0;
    fMapEnabled = isWindows && !!UV_FS_O_FILEMAP;
    fMapLimit = 512 * 1024;
    fMapFlag = UV_FS_O_FILEMAP | O_TRUNC | O_CREAT | O_WRONLY;
    noFollowFlag = !isWindows && typeof O_NOFOLLOW === "number" ? O_NOFOLLOW | O_TRUNC | O_CREAT | O_WRONLY : null;
    getWriteFlag = noFollowFlag !== null ? () => noFollowFlag : !fMapEnabled ? () => "w" : (size) => size < fMapLimit ? fMapFlag : "w";
  }
});

// node_modules/chownr/dist/esm/index.js
var import_node_fs2, import_node_path3, lchownSync, chown, chownrKid, chownr, chownrKidSync, chownrSync;
var init_esm4 = __esm({
  "node_modules/chownr/dist/esm/index.js"() {
    import_node_fs2 = __toESM(require("node:fs"), 1);
    import_node_path3 = __toESM(require("node:path"), 1);
    lchownSync = (path16, uid, gid) => {
      try {
        return import_node_fs2.default.lchownSync(path16, uid, gid);
      } catch (er) {
        if (er?.code !== "ENOENT")
          throw er;
      }
    };
    chown = (cpath, uid, gid, cb) => {
      import_node_fs2.default.lchown(cpath, uid, gid, (er) => {
        cb(er && er?.code !== "ENOENT" ? er : null);
      });
    };
    chownrKid = (p, child, uid, gid, cb) => {
      if (child.isDirectory()) {
        chownr(import_node_path3.default.resolve(p, child.name), uid, gid, (er) => {
          if (er)
            return cb(er);
          const cpath = import_node_path3.default.resolve(p, child.name);
          chown(cpath, uid, gid, cb);
        });
      } else {
        const cpath = import_node_path3.default.resolve(p, child.name);
        chown(cpath, uid, gid, cb);
      }
    };
    chownr = (p, uid, gid, cb) => {
      import_node_fs2.default.readdir(p, { withFileTypes: true }, (er, children) => {
        if (er) {
          if (er.code === "ENOENT")
            return cb();
          else if (er.code !== "ENOTDIR" && er.code !== "ENOTSUP")
            return cb(er);
        }
        if (er || !children.length)
          return chown(p, uid, gid, cb);
        let len = children.length;
        let errState = null;
        const then = (er2) => {
          if (errState)
            return;
          if (er2)
            return cb(errState = er2);
          if (--len === 0)
            return chown(p, uid, gid, cb);
        };
        for (const child of children) {
          chownrKid(p, child, uid, gid, then);
        }
      });
    };
    chownrKidSync = (p, child, uid, gid) => {
      if (child.isDirectory())
        chownrSync(import_node_path3.default.resolve(p, child.name), uid, gid);
      lchownSync(import_node_path3.default.resolve(p, child.name), uid, gid);
    };
    chownrSync = (p, uid, gid) => {
      let children;
      try {
        children = import_node_fs2.default.readdirSync(p, { withFileTypes: true });
      } catch (er) {
        const e = er;
        if (e?.code === "ENOENT")
          return;
        else if (e?.code === "ENOTDIR" || e?.code === "ENOTSUP")
          return lchownSync(p, uid, gid);
        else
          throw e;
      }
      for (const child of children) {
        chownrKidSync(p, child, uid, gid);
      }
      return lchownSync(p, uid, gid);
    };
  }
});

// node_modules/tar/dist/esm/cwd-error.js
var CwdError;
var init_cwd_error = __esm({
  "node_modules/tar/dist/esm/cwd-error.js"() {
    CwdError = class extends Error {
      path;
      code;
      syscall = "chdir";
      constructor(path16, code2) {
        super(`${code2}: Cannot cd into '${path16}'`);
        this.path = path16;
        this.code = code2;
      }
      get name() {
        return "CwdError";
      }
    };
  }
});

// node_modules/tar/dist/esm/symlink-error.js
var SymlinkError;
var init_symlink_error = __esm({
  "node_modules/tar/dist/esm/symlink-error.js"() {
    SymlinkError = class extends Error {
      path;
      symlink;
      syscall = "symlink";
      code = "TAR_SYMLINK_ERROR";
      constructor(symlink, path16) {
        super("TAR_SYMLINK_ERROR: Cannot extract through symbolic link");
        this.symlink = symlink;
        this.path = path16;
      }
      get name() {
        return "SymlinkError";
      }
    };
  }
});

// node_modules/tar/dist/esm/mkdir.js
var import_node_fs3, import_promises, import_node_path4, checkCwd, mkdir, mkdir_, onmkdir, checkCwdSync, mkdirSync2;
var init_mkdir = __esm({
  "node_modules/tar/dist/esm/mkdir.js"() {
    init_esm4();
    import_node_fs3 = __toESM(require("node:fs"), 1);
    import_promises = __toESM(require("node:fs/promises"), 1);
    import_node_path4 = __toESM(require("node:path"), 1);
    init_cwd_error();
    init_normalize_windows_path();
    init_symlink_error();
    checkCwd = (dir, cb) => {
      import_node_fs3.default.stat(dir, (er, st) => {
        if (er || !st.isDirectory()) {
          er = new CwdError(dir, er?.code || "ENOTDIR");
        }
        cb(er);
      });
    };
    mkdir = (dir, opt, cb) => {
      dir = normalizeWindowsPath(dir);
      const umask2 = opt.umask ?? 18;
      const mode = opt.mode | 448;
      const needChmod = (mode & umask2) !== 0;
      const uid = opt.uid;
      const gid = opt.gid;
      const doChown = typeof uid === "number" && typeof gid === "number" && (uid !== opt.processUid || gid !== opt.processGid);
      const preserve = opt.preserve;
      const unlink = opt.unlink;
      const cwd = normalizeWindowsPath(opt.cwd);
      const done = (er, created) => {
        if (er) {
          cb(er);
        } else {
          if (created && doChown) {
            chownr(created, uid, gid, (er2) => done(er2));
          } else if (needChmod) {
            import_node_fs3.default.chmod(dir, mode, cb);
          } else {
            cb();
          }
        }
      };
      if (dir === cwd) {
        return checkCwd(dir, done);
      }
      if (preserve) {
        return import_promises.default.mkdir(dir, { mode, recursive: true }).then(
          (made) => done(null, made ?? void 0),
          // oh, ts
          done
        );
      }
      const sub = normalizeWindowsPath(import_node_path4.default.relative(cwd, dir));
      const parts = sub.split("/");
      mkdir_(cwd, parts, mode, unlink, cwd, void 0, done);
    };
    mkdir_ = (base, parts, mode, unlink, cwd, created, cb) => {
      if (parts.length === 0) {
        return cb(null, created);
      }
      const p = parts.shift();
      const part = normalizeWindowsPath(import_node_path4.default.resolve(base + "/" + p));
      import_node_fs3.default.mkdir(part, mode, onmkdir(part, parts, mode, unlink, cwd, created, cb));
    };
    onmkdir = (part, parts, mode, unlink, cwd, created, cb) => (er) => {
      if (er) {
        import_node_fs3.default.lstat(part, (statEr, st) => {
          if (statEr) {
            statEr.path = statEr.path && normalizeWindowsPath(statEr.path);
            cb(statEr);
          } else if (st.isDirectory()) {
            mkdir_(part, parts, mode, unlink, cwd, created, cb);
          } else if (unlink) {
            import_node_fs3.default.unlink(part, (er2) => {
              if (er2) {
                return cb(er2);
              }
              import_node_fs3.default.mkdir(part, mode, onmkdir(part, parts, mode, unlink, cwd, created, cb));
            });
          } else if (st.isSymbolicLink()) {
            return cb(new SymlinkError(part, part + "/" + parts.join("/")));
          } else {
            cb(er);
          }
        });
      } else {
        created = created || part;
        mkdir_(part, parts, mode, unlink, cwd, created, cb);
      }
    };
    checkCwdSync = (dir) => {
      let ok = false;
      let code2;
      try {
        ok = import_node_fs3.default.statSync(dir).isDirectory();
      } catch (er) {
        code2 = er?.code;
      } finally {
        if (!ok) {
          throw new CwdError(dir, code2 ?? "ENOTDIR");
        }
      }
    };
    mkdirSync2 = (dir, opt) => {
      dir = normalizeWindowsPath(dir);
      const umask2 = opt.umask ?? 18;
      const mode = opt.mode | 448;
      const needChmod = (mode & umask2) !== 0;
      const uid = opt.uid;
      const gid = opt.gid;
      const doChown = typeof uid === "number" && typeof gid === "number" && (uid !== opt.processUid || gid !== opt.processGid);
      const preserve = opt.preserve;
      const unlink = opt.unlink;
      const cwd = normalizeWindowsPath(opt.cwd);
      const done = (created2) => {
        if (created2 && doChown) {
          chownrSync(created2, uid, gid);
        }
        if (needChmod) {
          import_node_fs3.default.chmodSync(dir, mode);
        }
      };
      if (dir === cwd) {
        checkCwdSync(cwd);
        return done();
      }
      if (preserve) {
        return done(import_node_fs3.default.mkdirSync(dir, { mode, recursive: true }) ?? void 0);
      }
      const sub = normalizeWindowsPath(import_node_path4.default.relative(cwd, dir));
      const parts = sub.split("/");
      let created;
      for (let p = parts.shift(), part = cwd; p && (part += "/" + p); p = parts.shift()) {
        part = normalizeWindowsPath(import_node_path4.default.resolve(part));
        try {
          import_node_fs3.default.mkdirSync(part, mode);
          created = created || part;
        } catch {
          const st = import_node_fs3.default.lstatSync(part);
          if (st.isDirectory()) {
            continue;
          } else if (unlink) {
            import_node_fs3.default.unlinkSync(part);
            import_node_fs3.default.mkdirSync(part, mode);
            created = created || part;
            continue;
          } else if (st.isSymbolicLink()) {
            return new SymlinkError(part, part + "/" + parts.join("/"));
          }
        }
      }
      return done(created);
    };
  }
});

// node_modules/tar/dist/esm/strip-absolute-path.js
var import_node_path5, isAbsolute, parse3, stripAbsolutePath;
var init_strip_absolute_path = __esm({
  "node_modules/tar/dist/esm/strip-absolute-path.js"() {
    import_node_path5 = require("node:path");
    ({ isAbsolute, parse: parse3 } = import_node_path5.win32);
    stripAbsolutePath = (path16) => {
      let r = "";
      let parsed = parse3(path16);
      while (isAbsolute(path16) || parsed.root) {
        const root = path16.charAt(0) === "/" && path16.slice(0, 4) !== "//?/" ? "/" : parsed.root;
        path16 = path16.slice(root.length);
        r += root;
        parsed = parse3(path16);
      }
      return [r, path16];
    };
  }
});

// node_modules/tar/dist/esm/winchars.js
var raw, win, toWin, toRaw, encode2, decode;
var init_winchars = __esm({
  "node_modules/tar/dist/esm/winchars.js"() {
    raw = ["|", "<", ">", "?", ":"];
    win = raw.map((char) => String.fromCodePoint(61440 + Number(char.codePointAt(0))));
    toWin = new Map(raw.map((char, i) => [char, win[i]]));
    toRaw = new Map(win.map((char, i) => [char, raw[i]]));
    encode2 = (s) => raw.reduce((s2, c) => s2.split(c).join(toWin.get(c)), s);
    decode = (s) => win.reduce((s2, c) => s2.split(c).join(toRaw.get(c)), s);
  }
});

// node_modules/tar/dist/esm/normalize-unicode.js
var normalizeCache, MAX, cache, normalizeUnicode;
var init_normalize_unicode = __esm({
  "node_modules/tar/dist/esm/normalize-unicode.js"() {
    normalizeCache = /* @__PURE__ */ Object.create(null);
    MAX = 1e4;
    cache = /* @__PURE__ */ new Set();
    normalizeUnicode = (s) => {
      if (!cache.has(s)) {
        normalizeCache[s] = s.normalize("NFD").toLocaleLowerCase("en").toLocaleUpperCase("en");
      } else {
        cache.delete(s);
      }
      cache.add(s);
      const ret = normalizeCache[s];
      let i = cache.size - MAX;
      if (i > MAX / 10) {
        for (const s2 of cache) {
          cache.delete(s2);
          delete normalizeCache[s2];
          if (--i <= 0)
            break;
        }
      }
      return ret;
    };
  }
});

// node_modules/tar/dist/esm/path-reservations.js
var import_node_path6, platform3, isWindows2, getDirs, PathReservations;
var init_path_reservations = __esm({
  "node_modules/tar/dist/esm/path-reservations.js"() {
    import_node_path6 = require("node:path");
    init_normalize_unicode();
    init_strip_trailing_slashes();
    platform3 = process.env.TESTING_TAR_FAKE_PLATFORM || process.platform;
    isWindows2 = platform3 === "win32";
    getDirs = (path16) => {
      const dirs = path16.split("/").slice(0, -1).reduce((set, path17) => {
        const s = set.at(-1);
        if (s !== void 0) {
          path17 = (0, import_node_path6.join)(s, path17);
        }
        set.push(path17 || "/");
        return set;
      }, []);
      return dirs;
    };
    PathReservations = class {
      // path => [function or Set]
      // A Set object means a directory reservation
      // A fn is a direct reservation on that path
      #queues = /* @__PURE__ */ new Map();
      // fn => {paths:[path,...], dirs:[path, ...]}
      #reservations = /* @__PURE__ */ new Map();
      // functions currently running
      #running = /* @__PURE__ */ new Set();
      reserve(paths, fn2) {
        paths = isWindows2 ? ["win32 parallelization disabled"] : paths.map((p) => {
          return stripTrailingSlashes((0, import_node_path6.join)(normalizeUnicode(p)));
        });
        const dirs = new Set(paths.map((path16) => getDirs(path16)).reduce((a, b) => a.concat(b)));
        this.#reservations.set(fn2, { dirs, paths });
        for (const p of paths) {
          const q = this.#queues.get(p);
          if (!q) {
            this.#queues.set(p, [fn2]);
          } else {
            q.push(fn2);
          }
        }
        for (const dir of dirs) {
          const q = this.#queues.get(dir);
          if (!q) {
            this.#queues.set(dir, [/* @__PURE__ */ new Set([fn2])]);
          } else {
            const l = q.at(-1);
            if (l instanceof Set) {
              l.add(fn2);
            } else {
              q.push(/* @__PURE__ */ new Set([fn2]));
            }
          }
        }
        return this.#run(fn2);
      }
      // return the queues for each path the function cares about
      // fn => {paths, dirs}
      #getQueues(fn2) {
        const res = this.#reservations.get(fn2);
        if (!res) {
          throw new Error("function does not have any path reservations");
        }
        return {
          paths: res.paths.map((path16) => this.#queues.get(path16)),
          dirs: [...res.dirs].map((path16) => this.#queues.get(path16))
        };
      }
      // check if fn is first in line for all its paths, and is
      // included in the first set for all its dir queues
      check(fn2) {
        const { paths, dirs } = this.#getQueues(fn2);
        return paths.every((q) => q && q[0] === fn2) && dirs.every((q) => q && q[0] instanceof Set && q[0].has(fn2));
      }
      // run the function if it's first in line and not already running
      #run(fn2) {
        if (this.#running.has(fn2) || !this.check(fn2)) {
          return false;
        }
        this.#running.add(fn2);
        fn2(() => this.#clear(fn2));
        return true;
      }
      #clear(fn2) {
        if (!this.#running.has(fn2)) {
          return false;
        }
        const res = this.#reservations.get(fn2);
        if (!res) {
          throw new Error("invalid reservation");
        }
        const { paths, dirs } = res;
        const next = /* @__PURE__ */ new Set();
        for (const path16 of paths) {
          const q = this.#queues.get(path16);
          if (!q || q?.[0] !== fn2) {
            continue;
          }
          const q0 = q[1];
          if (!q0) {
            this.#queues.delete(path16);
            continue;
          }
          q.shift();
          if (typeof q0 === "function") {
            next.add(q0);
          } else {
            for (const f of q0) {
              next.add(f);
            }
          }
        }
        for (const dir of dirs) {
          const q = this.#queues.get(dir);
          const q0 = q?.[0];
          if (!q || !(q0 instanceof Set))
            continue;
          if (q0.size === 1 && q.length === 1) {
            this.#queues.delete(dir);
            continue;
          } else if (q0.size === 1) {
            q.shift();
            const n = q[0];
            if (typeof n === "function") {
              next.add(n);
            }
          } else {
            q0.delete(fn2);
          }
        }
        this.#running.delete(fn2);
        next.forEach((fn3) => this.#run(fn3));
        return true;
      }
    };
  }
});

// node_modules/tar/dist/esm/process-umask.js
var umask;
var init_process_umask = __esm({
  "node_modules/tar/dist/esm/process-umask.js"() {
    umask = () => process.umask();
  }
});

// node_modules/tar/dist/esm/unpack.js
var import_node_assert, import_node_crypto, import_node_fs4, import_node_path7, ONENTRY, CHECKFS, CHECKFS2, ISREUSABLE, MAKEFS, FILE, DIRECTORY, LINK, SYMLINK, HARDLINK, ENSURE_NO_SYMLINK, UNSUPPORTED, CHECKPATH, STRIPABSOLUTEPATH, MKDIR, ONERROR, PENDING, PEND, UNPEND, ENDED2, MAYBECLOSE, SKIP, DOCHOWN, UID, GID, CHECKED_CWD, platform4, isWindows3, DEFAULT_MAX_DEPTH, unlinkFile, unlinkFileSync, uint32, Unpack, callSync, UnpackSync;
var init_unpack = __esm({
  "node_modules/tar/dist/esm/unpack.js"() {
    init_esm2();
    import_node_assert = __toESM(require("node:assert"), 1);
    import_node_crypto = require("node:crypto");
    import_node_fs4 = __toESM(require("node:fs"), 1);
    import_node_path7 = __toESM(require("node:path"), 1);
    init_get_write_flag();
    init_mkdir();
    init_normalize_windows_path();
    init_parse();
    init_strip_absolute_path();
    init_winchars();
    init_path_reservations();
    init_symlink_error();
    init_process_umask();
    ONENTRY = /* @__PURE__ */ Symbol("onEntry");
    CHECKFS = /* @__PURE__ */ Symbol("checkFs");
    CHECKFS2 = /* @__PURE__ */ Symbol("checkFs2");
    ISREUSABLE = /* @__PURE__ */ Symbol("isReusable");
    MAKEFS = /* @__PURE__ */ Symbol("makeFs");
    FILE = /* @__PURE__ */ Symbol("file");
    DIRECTORY = /* @__PURE__ */ Symbol("directory");
    LINK = /* @__PURE__ */ Symbol("link");
    SYMLINK = /* @__PURE__ */ Symbol("symlink");
    HARDLINK = /* @__PURE__ */ Symbol("hardlink");
    ENSURE_NO_SYMLINK = /* @__PURE__ */ Symbol("ensureNoSymlink");
    UNSUPPORTED = /* @__PURE__ */ Symbol("unsupported");
    CHECKPATH = /* @__PURE__ */ Symbol("checkPath");
    STRIPABSOLUTEPATH = /* @__PURE__ */ Symbol("stripAbsolutePath");
    MKDIR = /* @__PURE__ */ Symbol("mkdir");
    ONERROR = /* @__PURE__ */ Symbol("onError");
    PENDING = /* @__PURE__ */ Symbol("pending");
    PEND = /* @__PURE__ */ Symbol("pend");
    UNPEND = /* @__PURE__ */ Symbol("unpend");
    ENDED2 = /* @__PURE__ */ Symbol("ended");
    MAYBECLOSE = /* @__PURE__ */ Symbol("maybeClose");
    SKIP = /* @__PURE__ */ Symbol("skip");
    DOCHOWN = /* @__PURE__ */ Symbol("doChown");
    UID = /* @__PURE__ */ Symbol("uid");
    GID = /* @__PURE__ */ Symbol("gid");
    CHECKED_CWD = /* @__PURE__ */ Symbol("checkedCwd");
    platform4 = process.env.TESTING_TAR_FAKE_PLATFORM || process.platform;
    isWindows3 = platform4 === "win32";
    DEFAULT_MAX_DEPTH = 1024;
    unlinkFile = (path16, cb) => {
      if (!isWindows3) {
        return import_node_fs4.default.unlink(path16, cb);
      }
      const name2 = path16 + ".DELETE." + (0, import_node_crypto.randomBytes)(16).toString("hex");
      import_node_fs4.default.rename(path16, name2, (er) => {
        if (er) {
          return cb(er);
        }
        import_node_fs4.default.unlink(name2, cb);
      });
    };
    unlinkFileSync = (path16) => {
      if (!isWindows3) {
        return import_node_fs4.default.unlinkSync(path16);
      }
      const name2 = path16 + ".DELETE." + (0, import_node_crypto.randomBytes)(16).toString("hex");
      import_node_fs4.default.renameSync(path16, name2);
      import_node_fs4.default.unlinkSync(name2);
    };
    uint32 = (a, b, c) => a !== void 0 && a === a >>> 0 ? a : b !== void 0 && b === b >>> 0 ? b : c;
    Unpack = class extends Parser {
      [ENDED2] = false;
      [CHECKED_CWD] = false;
      [PENDING] = 0;
      reservations = new PathReservations();
      transform;
      writable = true;
      readable = false;
      uid;
      gid;
      setOwner;
      preserveOwner;
      processGid;
      processUid;
      maxDepth;
      forceChown;
      win32;
      newer;
      keep;
      noMtime;
      preservePaths;
      unlink;
      cwd;
      strip;
      processUmask;
      umask;
      dmode;
      fmode;
      chmod;
      constructor(opt = {}) {
        opt.ondone = () => {
          this[ENDED2] = true;
          this[MAYBECLOSE]();
        };
        super(opt);
        this.transform = opt.transform;
        this.chmod = !!opt.chmod;
        if (typeof opt.uid === "number" || typeof opt.gid === "number") {
          if (typeof opt.uid !== "number" || typeof opt.gid !== "number") {
            throw new TypeError("cannot set owner without number uid and gid");
          }
          if (opt.preserveOwner) {
            throw new TypeError("cannot preserve owner in archive and also set owner explicitly");
          }
          this.uid = opt.uid;
          this.gid = opt.gid;
          this.setOwner = true;
        } else {
          this.uid = void 0;
          this.gid = void 0;
          this.setOwner = false;
        }
        this.preserveOwner = opt.preserveOwner === void 0 && typeof opt.uid !== "number" ? !!(process.getuid && process.getuid() === 0) : !!opt.preserveOwner;
        this.processUid = (this.preserveOwner || this.setOwner) && process.getuid ? process.getuid() : void 0;
        this.processGid = (this.preserveOwner || this.setOwner) && process.getgid ? process.getgid() : void 0;
        this.maxDepth = typeof opt.maxDepth === "number" ? opt.maxDepth : DEFAULT_MAX_DEPTH;
        this.forceChown = opt.forceChown === true;
        this.win32 = !!opt.win32 || isWindows3;
        this.newer = !!opt.newer;
        this.keep = !!opt.keep;
        this.noMtime = !!opt.noMtime;
        this.preservePaths = !!opt.preservePaths;
        this.unlink = !!opt.unlink;
        this.cwd = normalizeWindowsPath(import_node_path7.default.resolve(opt.cwd || process.cwd()));
        this.strip = Number(opt.strip) || 0;
        this.processUmask = !this.chmod ? 0 : typeof opt.processUmask === "number" ? opt.processUmask : umask();
        this.umask = typeof opt.umask === "number" ? opt.umask : this.processUmask;
        this.dmode = opt.dmode || 511 & ~this.umask;
        this.fmode = opt.fmode || 438 & ~this.umask;
        this.on("entry", (entry) => this[ONENTRY](entry));
      }
      // a bad or damaged archive is a warning for Parser, but an error
      // when extracting.  Mark those errors as unrecoverable, because
      // the Unpack contract cannot be met.
      warn(code2, msg, data = {}) {
        if (code2 === "TAR_BAD_ARCHIVE" || code2 === "TAR_ABORT") {
          data.recoverable = false;
        }
        return super.warn(code2, msg, data);
      }
      [MAYBECLOSE]() {
        if (this[ENDED2] && this[PENDING] === 0) {
          this.emit("prefinish");
          this.emit("finish");
          this.emit("end");
        }
      }
      // return false if we need to skip this file
      // return true if the field was successfully sanitized
      [STRIPABSOLUTEPATH](entry, field) {
        const p = entry[field];
        const { type } = entry;
        if (!p || this.preservePaths)
          return true;
        const [root, stripped] = stripAbsolutePath(p);
        const parts = stripped.replaceAll(/\\/g, "/").split("/");
        if (parts.includes("..") || /* c8 ignore next */
        isWindows3 && /^[a-z]:\.\.$/i.test(parts[0] ?? "")) {
          if (field === "path" || type === "Link") {
            this.warn("TAR_ENTRY_ERROR", `${field} contains '..'`, {
              entry,
              [field]: p
            });
            return false;
          }
          const entryDir = import_node_path7.default.posix.dirname(entry.path);
          const resolved = import_node_path7.default.posix.normalize(import_node_path7.default.posix.join(entryDir, parts.join("/")));
          if (resolved.startsWith("../") || resolved === "..") {
            this.warn("TAR_ENTRY_ERROR", `${field} escapes extraction directory`, {
              entry,
              [field]: p
            });
            return false;
          }
        }
        if (root) {
          entry[field] = String(stripped);
          this.warn("TAR_ENTRY_INFO", `stripping ${root} from absolute ${field}`, {
            entry,
            [field]: p
          });
        }
        return true;
      }
      // no IO, just string checking for absolute indicators
      [CHECKPATH](entry) {
        const p = normalizeWindowsPath(entry.path);
        const parts = p.split("/");
        if (this.strip) {
          if (parts.length < this.strip) {
            return false;
          }
          if (entry.type === "Link") {
            const linkparts = normalizeWindowsPath(String(entry.linkpath)).split("/");
            if (linkparts.length >= this.strip) {
              entry.linkpath = linkparts.slice(this.strip).join("/");
            } else {
              return false;
            }
          }
          parts.splice(0, this.strip);
          entry.path = parts.join("/");
        }
        if (isFinite(this.maxDepth) && parts.length > this.maxDepth) {
          this.warn("TAR_ENTRY_ERROR", "path excessively deep", {
            entry,
            path: p,
            depth: parts.length,
            maxDepth: this.maxDepth
          });
          return false;
        }
        if (!this[STRIPABSOLUTEPATH](entry, "path") || !this[STRIPABSOLUTEPATH](entry, "linkpath")) {
          return false;
        }
        entry.absolute = import_node_path7.default.isAbsolute(entry.path) ? normalizeWindowsPath(import_node_path7.default.resolve(entry.path)) : normalizeWindowsPath(import_node_path7.default.resolve(this.cwd, entry.path));
        if (!this.preservePaths && typeof entry.absolute === "string" && entry.absolute.indexOf(this.cwd + "/") !== 0 && entry.absolute !== this.cwd) {
          this.warn("TAR_ENTRY_ERROR", "path escaped extraction target", {
            entry,
            path: normalizeWindowsPath(entry.path),
            resolvedPath: entry.absolute,
            cwd: this.cwd
          });
          return false;
        }
        if (entry.absolute === this.cwd && entry.type !== "Directory" && entry.type !== "GNUDumpDir") {
          return false;
        }
        if (this.win32) {
          const { root: aRoot } = import_node_path7.default.win32.parse(String(entry.absolute));
          entry.absolute = aRoot + encode2(String(entry.absolute).slice(aRoot.length));
          const { root: pRoot } = import_node_path7.default.win32.parse(entry.path);
          entry.path = pRoot + encode2(entry.path.slice(pRoot.length));
        }
        return true;
      }
      [ONENTRY](entry) {
        if (!this[CHECKPATH](entry)) {
          return entry.resume();
        }
        import_node_assert.default.equal(typeof entry.absolute, "string");
        switch (entry.type) {
          case "Directory":
          case "GNUDumpDir":
            if (entry.mode) {
              entry.mode = entry.mode | 448;
            }
          // eslint-disable-next-line no-fallthrough
          case "File":
          case "OldFile":
          case "ContiguousFile":
          case "Link":
          case "SymbolicLink":
            return this[CHECKFS](entry);
          case "CharacterDevice":
          case "BlockDevice":
          case "FIFO":
          default:
            return this[UNSUPPORTED](entry);
        }
      }
      [ONERROR](er, entry) {
        if (er.name === "CwdError") {
          this.emit("error", er);
        } else {
          this.warn("TAR_ENTRY_ERROR", er, { entry });
          this[UNPEND]();
          entry.resume();
        }
      }
      [MKDIR](dir, mode, cb) {
        mkdir(normalizeWindowsPath(dir), {
          uid: this.uid,
          gid: this.gid,
          processUid: this.processUid,
          processGid: this.processGid,
          umask: this.processUmask,
          preserve: this.preservePaths,
          unlink: this.unlink,
          cwd: this.cwd,
          mode
        }, cb);
      }
      [DOCHOWN](entry) {
        return this.forceChown || this.preserveOwner && (typeof entry.uid === "number" && entry.uid !== this.processUid || typeof entry.gid === "number" && entry.gid !== this.processGid) || typeof this.uid === "number" && this.uid !== this.processUid || typeof this.gid === "number" && this.gid !== this.processGid;
      }
      [UID](entry) {
        return uint32(this.uid, entry.uid, this.processUid);
      }
      [GID](entry) {
        return uint32(this.gid, entry.gid, this.processGid);
      }
      [FILE](entry, fullyDone) {
        const mode = typeof entry.mode === "number" ? entry.mode & 4095 : this.fmode;
        const stream = new WriteStream(String(entry.absolute), {
          // slight lie, but it can be numeric flags
          flags: getWriteFlag(entry.size),
          mode,
          autoClose: false
        });
        stream.on("error", (er) => {
          if (stream.fd) {
            import_node_fs4.default.close(stream.fd, () => {
            });
          }
          stream.write = () => true;
          this[ONERROR](er, entry);
          fullyDone();
        });
        let actions = 1;
        const done = (er) => {
          if (er) {
            if (stream.fd) {
              import_node_fs4.default.close(stream.fd, () => {
              });
            }
            this[ONERROR](er, entry);
            fullyDone();
            return;
          }
          if (--actions === 0) {
            if (stream.fd !== void 0) {
              import_node_fs4.default.close(stream.fd, (er2) => {
                if (er2) {
                  this[ONERROR](er2, entry);
                } else {
                  this[UNPEND]();
                }
                fullyDone();
              });
            }
          }
        };
        stream.on("finish", () => {
          const abs = String(entry.absolute);
          const fd = stream.fd;
          if (typeof fd === "number" && entry.mtime && !this.noMtime) {
            actions++;
            const atime = entry.atime || /* @__PURE__ */ new Date();
            const mtime = entry.mtime;
            import_node_fs4.default.futimes(fd, atime, mtime, (er) => er ? import_node_fs4.default.utimes(abs, atime, mtime, (er2) => done(er2 && er)) : done());
          }
          if (typeof fd === "number" && this[DOCHOWN](entry)) {
            actions++;
            const uid = this[UID](entry);
            const gid = this[GID](entry);
            if (typeof uid === "number" && typeof gid === "number") {
              import_node_fs4.default.fchown(fd, uid, gid, (er) => er ? import_node_fs4.default.chown(abs, uid, gid, (er2) => done(er2 && er)) : done());
            }
          }
          done();
        });
        const tx = this.transform ? this.transform(entry) || entry : entry;
        if (tx !== entry) {
          tx.on("error", (er) => {
            this[ONERROR](er, entry);
            fullyDone();
          });
          entry.pipe(tx);
        }
        tx.pipe(stream);
      }
      [DIRECTORY](entry, fullyDone) {
        const mode = typeof entry.mode === "number" ? entry.mode & 4095 : this.dmode;
        this[MKDIR](String(entry.absolute), mode, (er) => {
          if (er) {
            this[ONERROR](er, entry);
            fullyDone();
            return;
          }
          let actions = 1;
          const done = () => {
            if (--actions === 0) {
              fullyDone();
              this[UNPEND]();
              entry.resume();
            }
          };
          if (entry.mtime && !this.noMtime) {
            actions++;
            import_node_fs4.default.utimes(String(entry.absolute), entry.atime || /* @__PURE__ */ new Date(), entry.mtime, done);
          }
          if (this[DOCHOWN](entry)) {
            actions++;
            import_node_fs4.default.chown(String(entry.absolute), Number(this[UID](entry)), Number(this[GID](entry)), done);
          }
          done();
        });
      }
      [UNSUPPORTED](entry) {
        entry.unsupported = true;
        this.warn("TAR_ENTRY_UNSUPPORTED", `unsupported entry type: ${entry.type}`, { entry });
        entry.resume();
      }
      [SYMLINK](entry, done) {
        const parts = normalizeWindowsPath(import_node_path7.default.relative(this.cwd, import_node_path7.default.resolve(import_node_path7.default.dirname(String(entry.absolute)), String(entry.linkpath)))).split("/");
        this[ENSURE_NO_SYMLINK](entry, this.cwd, parts, () => this[LINK](entry, String(entry.linkpath), "symlink", done), (er) => {
          this[ONERROR](er, entry);
          done();
        });
      }
      [HARDLINK](entry, done) {
        const linkpath = normalizeWindowsPath(import_node_path7.default.resolve(this.cwd, String(entry.linkpath)));
        const parts = normalizeWindowsPath(String(entry.linkpath)).split("/");
        this[ENSURE_NO_SYMLINK](entry, this.cwd, parts, () => this[LINK](entry, linkpath, "link", done), (er) => {
          this[ONERROR](er, entry);
          done();
        });
      }
      [ENSURE_NO_SYMLINK](entry, cwd, parts, done, onError) {
        const p = parts.shift();
        if (this.preservePaths || p === void 0)
          return done();
        const t = import_node_path7.default.resolve(cwd, p);
        import_node_fs4.default.lstat(t, (er, st) => {
          if (er)
            return done();
          if (st?.isSymbolicLink()) {
            return onError(new SymlinkError(t, import_node_path7.default.resolve(t, parts.join("/"))));
          }
          this[ENSURE_NO_SYMLINK](entry, t, parts, done, onError);
        });
      }
      [PEND]() {
        this[PENDING]++;
      }
      [UNPEND]() {
        this[PENDING]--;
        this[MAYBECLOSE]();
      }
      [SKIP](entry) {
        this[UNPEND]();
        entry.resume();
      }
      // Check if we can reuse an existing filesystem entry safely and
      // overwrite it, rather than unlinking and recreating
      // Windows doesn't report a useful nlink, so we just never reuse entries
      [ISREUSABLE](entry, st) {
        return entry.type === "File" && !this.unlink && st.isFile() && st.nlink <= 1 && !isWindows3;
      }
      // check if a thing is there, and if so, try to clobber it
      [CHECKFS](entry) {
        this[PEND]();
        const paths = [entry.path];
        if (entry.linkpath) {
          paths.push(entry.linkpath);
        }
        this.reservations.reserve(paths, (done) => this[CHECKFS2](entry, done));
      }
      [CHECKFS2](entry, fullyDone) {
        const done = (er) => {
          fullyDone(er);
        };
        const checkCwd2 = () => {
          this[MKDIR](this.cwd, this.dmode, (er) => {
            if (er) {
              this[ONERROR](er, entry);
              done();
              return;
            }
            this[CHECKED_CWD] = true;
            start();
          });
        };
        const start = () => {
          if (entry.absolute !== this.cwd) {
            const parent = normalizeWindowsPath(import_node_path7.default.dirname(String(entry.absolute)));
            if (parent !== this.cwd) {
              return this[MKDIR](parent, this.dmode, (er) => {
                if (er) {
                  this[ONERROR](er, entry);
                  done();
                  return;
                }
                afterMakeParent();
              });
            }
          }
          afterMakeParent();
        };
        const afterMakeParent = () => {
          import_node_fs4.default.lstat(String(entry.absolute), (lstatEr, st) => {
            if (st && (this.keep || /* c8 ignore next */
            this.newer && st.mtime > (entry.mtime ?? st.mtime))) {
              this[SKIP](entry);
              done();
              return;
            }
            if (lstatEr || this[ISREUSABLE](entry, st)) {
              return this[MAKEFS](null, entry, done);
            }
            if (st.isDirectory()) {
              if (entry.type === "Directory") {
                const needChmod = this.chmod && entry.mode && (st.mode & 4095) !== entry.mode;
                const afterChmod = (er) => this[MAKEFS](er ?? null, entry, done);
                if (!needChmod) {
                  return afterChmod();
                }
                return import_node_fs4.default.chmod(String(entry.absolute), Number(entry.mode), afterChmod);
              }
              if (entry.absolute !== this.cwd) {
                return import_node_fs4.default.rmdir(String(entry.absolute), (er) => this[MAKEFS](er ?? null, entry, done));
              }
            }
            if (entry.absolute === this.cwd) {
              return this[MAKEFS](null, entry, done);
            }
            unlinkFile(String(entry.absolute), (er) => this[MAKEFS](er ?? null, entry, done));
          });
        };
        if (this[CHECKED_CWD]) {
          start();
        } else {
          checkCwd2();
        }
      }
      [MAKEFS](er, entry, done) {
        if (er) {
          this[ONERROR](er, entry);
          done();
          return;
        }
        switch (entry.type) {
          case "File":
          case "OldFile":
          case "ContiguousFile":
            return this[FILE](entry, done);
          case "Link":
            return this[HARDLINK](entry, done);
          case "SymbolicLink":
            return this[SYMLINK](entry, done);
          case "Directory":
          case "GNUDumpDir":
            return this[DIRECTORY](entry, done);
        }
      }
      [LINK](entry, linkpath, link, done) {
        import_node_fs4.default[link](linkpath, String(entry.absolute), (er) => {
          if (er) {
            this[ONERROR](er, entry);
          } else {
            this[UNPEND]();
            entry.resume();
          }
          done();
        });
      }
    };
    callSync = (fn2) => {
      try {
        return [null, fn2()];
      } catch (er) {
        return [er, null];
      }
    };
    UnpackSync = class extends Unpack {
      sync = true;
      [MAKEFS](er, entry) {
        return super[MAKEFS](er, entry, () => {
        });
      }
      [CHECKFS](entry) {
        if (!this[CHECKED_CWD]) {
          const er2 = this[MKDIR](this.cwd, this.dmode);
          if (er2) {
            return this[ONERROR](er2, entry);
          }
          this[CHECKED_CWD] = true;
        }
        if (entry.absolute !== this.cwd) {
          const parent = normalizeWindowsPath(import_node_path7.default.dirname(String(entry.absolute)));
          if (parent !== this.cwd) {
            const mkParent = this[MKDIR](parent, this.dmode);
            if (mkParent) {
              return this[ONERROR](mkParent, entry);
            }
          }
        }
        const [lstatEr, st] = callSync(() => import_node_fs4.default.lstatSync(String(entry.absolute)));
        if (st && (this.keep || /* c8 ignore next */
        this.newer && st.mtime > (entry.mtime ?? st.mtime))) {
          return this[SKIP](entry);
        }
        if (lstatEr || this[ISREUSABLE](entry, st)) {
          return this[MAKEFS](null, entry);
        }
        if (st.isDirectory()) {
          if (entry.type === "Directory") {
            const needChmod = this.chmod && entry.mode && (st.mode & 4095) !== entry.mode;
            const [er3] = needChmod ? callSync(() => {
              import_node_fs4.default.chmodSync(String(entry.absolute), Number(entry.mode));
            }) : [];
            return this[MAKEFS](er3, entry);
          }
          const [er2] = callSync(() => import_node_fs4.default.rmdirSync(String(entry.absolute)));
          this[MAKEFS](er2, entry);
        }
        const [er] = entry.absolute === this.cwd ? [] : callSync(() => unlinkFileSync(String(entry.absolute)));
        this[MAKEFS](er, entry);
      }
      [FILE](entry, done) {
        const mode = typeof entry.mode === "number" ? entry.mode & 4095 : this.fmode;
        const oner = (er) => {
          let closeError;
          try {
            import_node_fs4.default.closeSync(fd);
          } catch (e) {
            closeError = e;
          }
          if (er || closeError) {
            this[ONERROR](er || closeError, entry);
          }
          done();
        };
        let fd;
        try {
          fd = import_node_fs4.default.openSync(String(entry.absolute), getWriteFlag(entry.size), mode);
        } catch (er) {
          return oner(er);
        }
        const tx = this.transform ? this.transform(entry) || entry : entry;
        if (tx !== entry) {
          tx.on("error", (er) => this[ONERROR](er, entry));
          entry.pipe(tx);
        }
        tx.on("data", (chunk) => {
          try {
            import_node_fs4.default.writeSync(fd, chunk, 0, chunk.length);
          } catch (er) {
            oner(er);
          }
        });
        tx.on("end", () => {
          let er = null;
          if (entry.mtime && !this.noMtime) {
            const atime = entry.atime || /* @__PURE__ */ new Date();
            const mtime = entry.mtime;
            try {
              import_node_fs4.default.futimesSync(fd, atime, mtime);
            } catch (futimeser) {
              try {
                import_node_fs4.default.utimesSync(String(entry.absolute), atime, mtime);
              } catch {
                er = futimeser;
              }
            }
          }
          if (this[DOCHOWN](entry)) {
            const uid = this[UID](entry);
            const gid = this[GID](entry);
            try {
              import_node_fs4.default.fchownSync(fd, Number(uid), Number(gid));
            } catch (fchowner) {
              try {
                import_node_fs4.default.chownSync(String(entry.absolute), Number(uid), Number(gid));
              } catch {
                er = er || fchowner;
              }
            }
          }
          oner(er);
        });
      }
      [DIRECTORY](entry, done) {
        const mode = typeof entry.mode === "number" ? entry.mode & 4095 : this.dmode;
        const er = this[MKDIR](String(entry.absolute), mode);
        if (er) {
          this[ONERROR](er, entry);
          done();
          return;
        }
        if (entry.mtime && !this.noMtime) {
          try {
            import_node_fs4.default.utimesSync(String(entry.absolute), entry.atime || /* @__PURE__ */ new Date(), entry.mtime);
          } catch {
          }
        }
        if (this[DOCHOWN](entry)) {
          try {
            import_node_fs4.default.chownSync(String(entry.absolute), Number(this[UID](entry)), Number(this[GID](entry)));
          } catch {
          }
        }
        done();
        entry.resume();
      }
      [MKDIR](dir, mode) {
        try {
          return mkdirSync2(normalizeWindowsPath(dir), {
            uid: this.uid,
            gid: this.gid,
            processUid: this.processUid,
            processGid: this.processGid,
            umask: this.processUmask,
            preserve: this.preservePaths,
            unlink: this.unlink,
            cwd: this.cwd,
            mode
          });
        } catch (er) {
          return er;
        }
      }
      [ENSURE_NO_SYMLINK](_entry, cwd, parts, done, onError) {
        if (this.preservePaths || parts.length === 0)
          return done();
        let t = cwd;
        for (const p of parts) {
          t = import_node_path7.default.resolve(t, p);
          const [er, st] = callSync(() => import_node_fs4.default.lstatSync(t));
          if (er)
            return done();
          if (st.isSymbolicLink()) {
            return onError(new SymlinkError(t, import_node_path7.default.resolve(cwd, parts.join("/"))));
          }
        }
        done();
      }
      [LINK](entry, linkpath, link, done) {
        const linkSync = `${link}Sync`;
        try {
          import_node_fs4.default[linkSync](linkpath, String(entry.absolute));
          done();
          entry.resume();
        } catch (er) {
          return this[ONERROR](er, entry);
        }
      }
    };
  }
});

// node_modules/tar/dist/esm/extract.js
var extract_exports = {};
__export(extract_exports, {
  extract: () => extract
});
var import_node_fs5, extractFileSync, extractFile, extract;
var init_extract = __esm({
  "node_modules/tar/dist/esm/extract.js"() {
    init_esm2();
    import_node_fs5 = __toESM(require("node:fs"), 1);
    init_list();
    init_make_command();
    init_unpack();
    extractFileSync = (opt) => {
      const u = new UnpackSync(opt);
      const file = opt.file;
      const stat = import_node_fs5.default.statSync(file);
      const readSize = opt.maxReadSize || 16 * 1024 * 1024;
      const stream = new ReadStreamSync(file, {
        readSize,
        size: stat.size
      });
      stream.pipe(u);
    };
    extractFile = (opt, _) => {
      const u = new Unpack(opt);
      const readSize = opt.maxReadSize || 16 * 1024 * 1024;
      const file = opt.file;
      const p = new Promise((resolve, reject) => {
        u.on("error", reject);
        u.on("close", resolve);
        import_node_fs5.default.stat(file, (er, stat) => {
          if (er) {
            reject(er);
          } else {
            const stream = new ReadStream(file, {
              readSize,
              size: stat.size
            });
            stream.on("error", reject);
            stream.pipe(u);
          }
        });
      });
      return p;
    };
    extract = makeCommand(extractFileSync, extractFile, (opt) => new UnpackSync(opt), (opt) => new Unpack(opt), (opt, files) => {
      if (files?.length)
        filesFilter(opt, files);
    });
  }
});

// node_modules/v8-compile-cache/v8-compile-cache.js
var require_v8_compile_cache = __commonJS({
  "node_modules/v8-compile-cache/v8-compile-cache.js"(exports2, module2) {
    "use strict";
    var Module2 = require("module");
    var crypto = require("crypto");
    var fs17 = require("fs");
    var path16 = require("path");
    var vm = require("vm");
    var os3 = require("os");
    var hasOwnProperty = Object.prototype.hasOwnProperty;
    var FileSystemBlobStore = class {
      constructor(directory, prefix) {
        const name2 = prefix ? slashEscape(prefix + ".") : "";
        this._blobFilename = path16.join(directory, name2 + "BLOB");
        this._mapFilename = path16.join(directory, name2 + "MAP");
        this._lockFilename = path16.join(directory, name2 + "LOCK");
        this._directory = directory;
        this._load();
      }
      has(key, invalidationKey) {
        if (hasOwnProperty.call(this._memoryBlobs, key)) {
          return this._invalidationKeys[key] === invalidationKey;
        } else if (hasOwnProperty.call(this._storedMap, key)) {
          return this._storedMap[key][0] === invalidationKey;
        }
        return false;
      }
      get(key, invalidationKey) {
        if (hasOwnProperty.call(this._memoryBlobs, key)) {
          if (this._invalidationKeys[key] === invalidationKey) {
            return this._memoryBlobs[key];
          }
        } else if (hasOwnProperty.call(this._storedMap, key)) {
          const mapping = this._storedMap[key];
          if (mapping[0] === invalidationKey) {
            return this._storedBlob.slice(mapping[1], mapping[2]);
          }
        }
      }
      set(key, invalidationKey, buffer) {
        this._invalidationKeys[key] = invalidationKey;
        this._memoryBlobs[key] = buffer;
        this._dirty = true;
      }
      delete(key) {
        if (hasOwnProperty.call(this._memoryBlobs, key)) {
          this._dirty = true;
          delete this._memoryBlobs[key];
        }
        if (hasOwnProperty.call(this._invalidationKeys, key)) {
          this._dirty = true;
          delete this._invalidationKeys[key];
        }
        if (hasOwnProperty.call(this._storedMap, key)) {
          this._dirty = true;
          delete this._storedMap[key];
        }
      }
      isDirty() {
        return this._dirty;
      }
      save() {
        const dump = this._getDump();
        const blobToStore = Buffer.concat(dump[0]);
        const mapToStore = JSON.stringify(dump[1]);
        try {
          mkdirpSync(this._directory);
          fs17.writeFileSync(this._lockFilename, "LOCK", { flag: "wx" });
        } catch (error) {
          return false;
        }
        try {
          fs17.writeFileSync(this._blobFilename, blobToStore);
          fs17.writeFileSync(this._mapFilename, mapToStore);
        } finally {
          fs17.unlinkSync(this._lockFilename);
        }
        return true;
      }
      _load() {
        try {
          this._storedBlob = fs17.readFileSync(this._blobFilename);
          this._storedMap = JSON.parse(fs17.readFileSync(this._mapFilename));
        } catch (e) {
          this._storedBlob = Buffer.alloc(0);
          this._storedMap = {};
        }
        this._dirty = false;
        this._memoryBlobs = {};
        this._invalidationKeys = {};
      }
      _getDump() {
        const buffers = [];
        const newMap = {};
        let offset = 0;
        function push2(key, invalidationKey, buffer) {
          buffers.push(buffer);
          newMap[key] = [invalidationKey, offset, offset + buffer.length];
          offset += buffer.length;
        }
        for (const key of Object.keys(this._memoryBlobs)) {
          const buffer = this._memoryBlobs[key];
          const invalidationKey = this._invalidationKeys[key];
          push2(key, invalidationKey, buffer);
        }
        for (const key of Object.keys(this._storedMap)) {
          if (hasOwnProperty.call(newMap, key)) continue;
          const mapping = this._storedMap[key];
          const buffer = this._storedBlob.slice(mapping[1], mapping[2]);
          push2(key, mapping[0], buffer);
        }
        return [buffers, newMap];
      }
    };
    var NativeCompileCache = class {
      constructor() {
        this._cacheStore = null;
        this._previousModuleCompile = null;
      }
      setCacheStore(cacheStore) {
        this._cacheStore = cacheStore;
      }
      install() {
        const self2 = this;
        const hasRequireResolvePaths = typeof require.resolve.paths === "function";
        this._previousModuleCompile = Module2.prototype._compile;
        Module2.prototype._compile = function(content, filename) {
          const mod = this;
          function require2(id) {
            return mod.require(id);
          }
          function resolve(request, options) {
            return Module2._resolveFilename(request, mod, false, options);
          }
          require2.resolve = resolve;
          if (hasRequireResolvePaths) {
            resolve.paths = function paths(request) {
              return Module2._resolveLookupPaths(request, mod, true);
            };
          }
          require2.main = process.mainModule;
          require2.extensions = Module2._extensions;
          require2.cache = Module2._cache;
          const dirname2 = path16.dirname(filename);
          const compiledWrapper = self2._moduleCompile(filename, content);
          const args = [mod.exports, require2, mod, filename, dirname2, process, global, Buffer];
          return compiledWrapper.apply(mod.exports, args);
        };
      }
      uninstall() {
        Module2.prototype._compile = this._previousModuleCompile;
      }
      _moduleCompile(filename, content) {
        var contLen = content.length;
        if (contLen >= 2) {
          if (content.charCodeAt(0) === 35 && content.charCodeAt(1) === 33) {
            if (contLen === 2) {
              content = "";
            } else {
              var i = 2;
              for (; i < contLen; ++i) {
                var code2 = content.charCodeAt(i);
                if (code2 === 10 || code2 === 13) break;
              }
              if (i === contLen) {
                content = "";
              } else {
                content = content.slice(i);
              }
            }
          }
        }
        var wrapper = Module2.wrap(content);
        var invalidationKey = crypto.createHash("sha1").update(content, "utf8").digest("hex");
        var buffer = this._cacheStore.get(filename, invalidationKey);
        var script = new vm.Script(wrapper, {
          filename,
          lineOffset: 0,
          displayErrors: true,
          cachedData: buffer,
          produceCachedData: true
        });
        if (script.cachedDataProduced) {
          this._cacheStore.set(filename, invalidationKey, script.cachedData);
        } else if (script.cachedDataRejected) {
          this._cacheStore.delete(filename);
        }
        var compiledWrapper = script.runInThisContext({
          filename,
          lineOffset: 0,
          columnOffset: 0,
          displayErrors: true
        });
        return compiledWrapper;
      }
    };
    function mkdirpSync(p_) {
      _mkdirpSync(path16.resolve(p_), 511);
    }
    function _mkdirpSync(p, mode) {
      try {
        fs17.mkdirSync(p, mode);
      } catch (err0) {
        if (err0.code === "ENOENT") {
          _mkdirpSync(path16.dirname(p));
          _mkdirpSync(p);
        } else {
          try {
            const stat = fs17.statSync(p);
            if (!stat.isDirectory()) {
              throw err0;
            }
          } catch (err1) {
            throw err0;
          }
        }
      }
    }
    function slashEscape(str) {
      const ESCAPE_LOOKUP = {
        "\\": "zB",
        ":": "zC",
        "/": "zS",
        "\0": "z0",
        "z": "zZ"
      };
      const ESCAPE_REGEX = /[\\:/\x00z]/g;
      return str.replace(ESCAPE_REGEX, (match) => ESCAPE_LOOKUP[match]);
    }
    function supportsCachedData() {
      const script = new vm.Script('""', { produceCachedData: true });
      return script.cachedDataProduced === true;
    }
    function getCacheDir() {
      const v8_compile_cache_cache_dir = process.env.V8_COMPILE_CACHE_CACHE_DIR;
      if (v8_compile_cache_cache_dir) {
        return v8_compile_cache_cache_dir;
      }
      const dirname2 = typeof process.getuid === "function" ? "v8-compile-cache-" + process.getuid() : "v8-compile-cache";
      const arch = process.arch;
      const version2 = typeof process.versions.v8 === "string" ? process.versions.v8 : typeof process.versions.chakracore === "string" ? "chakracore-" + process.versions.chakracore : "node-" + process.version;
      const cacheDir = path16.join(os3.tmpdir(), dirname2, arch, version2);
      return cacheDir;
    }
    function getMainName() {
      const mainName = require.main && typeof require.main.filename === "string" ? require.main.filename : process.cwd();
      return mainName;
    }
    if (!process.env.DISABLE_V8_COMPILE_CACHE && supportsCachedData()) {
      const cacheDir = getCacheDir();
      const prefix = getMainName();
      const blobStore = new FileSystemBlobStore(cacheDir, prefix);
      const nativeCompileCache = new NativeCompileCache();
      nativeCompileCache.setCacheStore(blobStore);
      nativeCompileCache.install();
      process.once("exit", () => {
        if (blobStore.isDirty()) {
          blobStore.save();
        }
        nativeCompileCache.uninstall();
      });
    }
    module2.exports.__TEST__ = {
      FileSystemBlobStore,
      NativeCompileCache,
      mkdirpSync,
      slashEscape,
      supportsCachedData,
      getCacheDir,
      getMainName
    };
  }
});

// node_modules/semver/functions/satisfies.js
var require_satisfies = __commonJS({
  "node_modules/semver/functions/satisfies.js"(exports2, module2) {
    "use strict";
    var Range3 = require_range();
    var satisfies = (version2, range, options) => {
      try {
        range = new Range3(range, options);
      } catch (er) {
        return false;
      }
      return range.test(version2);
    };
    module2.exports = satisfies;
  }
});

// node_modules/which/node_modules/isexe/dist/commonjs/index.min.js
var require_index_min = __commonJS({
  "node_modules/which/node_modules/isexe/dist/commonjs/index.min.js"(exports2) {
    "use strict";
    var a = (t, e) => () => (e || t((e = { exports: {} }).exports, e), e.exports);
    var _ = a((i) => {
      "use strict";
      Object.defineProperty(i, "__esModule", { value: true });
      i.sync = i.isexe = void 0;
      var M = require("node:fs"), x = require("node:fs/promises"), q = async (t, e = {}) => {
        let { ignoreErrors: r = false } = e;
        try {
          return d(await (0, x.stat)(t), e);
        } catch (s) {
          let n = s;
          if (r || n.code === "EACCES") return false;
          throw n;
        }
      };
      i.isexe = q;
      var m = (t, e = {}) => {
        let { ignoreErrors: r = false } = e;
        try {
          return d((0, M.statSync)(t), e);
        } catch (s) {
          let n = s;
          if (r || n.code === "EACCES") return false;
          throw n;
        }
      };
      i.sync = m;
      var d = (t, e) => t.isFile() && A(t, e), A = (t, e) => {
        let r = e.uid ?? process.getuid?.(), s = e.groups ?? process.getgroups?.() ?? [], n = e.gid ?? process.getgid?.() ?? s[0];
        if (r === void 0 || n === void 0) throw new Error("cannot get uid or gid");
        let u = /* @__PURE__ */ new Set([n, ...s]), c = t.mode, S = t.uid, P = t.gid, f = parseInt("100", 8), l = parseInt("010", 8), j = parseInt("001", 8), C = f | l;
        return !!(c & j || c & l && u.has(P) || c & f && S === r || c & C && r === 0);
      };
    });
    var g = a((o) => {
      "use strict";
      Object.defineProperty(o, "__esModule", { value: true });
      o.sync = o.isexe = void 0;
      var T = require("node:fs"), I = require("node:fs/promises"), D = require("node:path"), F = async (t, e = {}) => {
        let { ignoreErrors: r = false } = e;
        try {
          return y(await (0, I.stat)(t), t, e);
        } catch (s) {
          let n = s;
          if (r || n.code === "EACCES") return false;
          throw n;
        }
      };
      o.isexe = F;
      var L = (t, e = {}) => {
        let { ignoreErrors: r = false } = e;
        try {
          return y((0, T.statSync)(t), t, e);
        } catch (s) {
          let n = s;
          if (r || n.code === "EACCES") return false;
          throw n;
        }
      };
      o.sync = L;
      var B = (t, e) => {
        let { pathExt: r = process.env.PATHEXT || "" } = e, s = r.split(D.delimiter);
        if (s.indexOf("") !== -1) return true;
        for (let n of s) {
          let u = n.toLowerCase(), c = t.substring(t.length - u.length).toLowerCase();
          if (u && c === u) return true;
        }
        return false;
      }, y = (t, e, r) => t.isFile() && B(e, r);
    });
    var p = a((h) => {
      "use strict";
      Object.defineProperty(h, "__esModule", { value: true });
    });
    var v = exports2 && exports2.__createBinding || (Object.create ? (function(t, e, r, s) {
      s === void 0 && (s = r);
      var n = Object.getOwnPropertyDescriptor(e, r);
      (!n || ("get" in n ? !e.__esModule : n.writable || n.configurable)) && (n = { enumerable: true, get: function() {
        return e[r];
      } }), Object.defineProperty(t, s, n);
    }) : (function(t, e, r, s) {
      s === void 0 && (s = r), t[s] = e[r];
    }));
    var G = exports2 && exports2.__setModuleDefault || (Object.create ? (function(t, e) {
      Object.defineProperty(t, "default", { enumerable: true, value: e });
    }) : function(t, e) {
      t.default = e;
    });
    var w = exports2 && exports2.__importStar || /* @__PURE__ */ (function() {
      var t = function(e) {
        return t = Object.getOwnPropertyNames || function(r) {
          var s = [];
          for (var n in r) Object.prototype.hasOwnProperty.call(r, n) && (s[s.length] = n);
          return s;
        }, t(e);
      };
      return function(e) {
        if (e && e.__esModule) return e;
        var r = {};
        if (e != null) for (var s = t(e), n = 0; n < s.length; n++) s[n] !== "default" && v(r, e, s[n]);
        return G(r, e), r;
      };
    })();
    var X = exports2 && exports2.__exportStar || function(t, e) {
      for (var r in t) r !== "default" && !Object.prototype.hasOwnProperty.call(e, r) && v(e, t, r);
    };
    Object.defineProperty(exports2, "__esModule", { value: true });
    exports2.sync = exports2.isexe = exports2.posix = exports2.win32 = void 0;
    var E = w(_());
    exports2.posix = E;
    var O = w(g());
    exports2.win32 = O;
    X(p(), exports2);
    var H = process.env._ISEXE_TEST_PLATFORM_ || process.platform;
    var b = H === "win32" ? O : E;
    exports2.isexe = b.isexe;
    exports2.sync = b.sync;
  }
});

// node_modules/which/lib/index.js
var require_lib = __commonJS({
  "node_modules/which/lib/index.js"(exports2, module2) {
    var { isexe, sync: isexeSync } = require_index_min();
    var { join: join3, delimiter, sep, posix } = require("path");
    var isWindows4 = process.platform === "win32";
    var rSlash = new RegExp(`[${posix.sep}${sep === posix.sep ? "" : sep}]`.replace(/(\\)/g, "\\$1"));
    var rRel = new RegExp(`^\\.${rSlash.source}`);
    var getNotFoundError = (cmd) => Object.assign(new Error(`not found: ${cmd}`), { code: "ENOENT" });
    var getPathInfo = (cmd, {
      path: optPath = process.env.PATH,
      pathExt: optPathExt = process.env.PATHEXT,
      delimiter: optDelimiter = delimiter
    }) => {
      const pathEnv = cmd.match(rSlash) ? [""] : [
        // windows always checks the cwd first
        ...isWindows4 ? [process.cwd()] : [],
        ...(optPath || /* istanbul ignore next: very unusual */
        "").split(optDelimiter)
      ];
      if (isWindows4) {
        const pathExtExe = optPathExt || [".EXE", ".CMD", ".BAT", ".COM"].join(optDelimiter);
        const pathExt = pathExtExe.split(optDelimiter).flatMap((item) => [item, item.toLowerCase()]);
        if (cmd.includes(".") && pathExt[0] !== "") {
          pathExt.unshift("");
        }
        return { pathEnv, pathExt, pathExtExe };
      }
      return { pathEnv, pathExt: [""] };
    };
    var getPathPart = (raw2, cmd) => {
      const pathPart = /^".*"$/.test(raw2) ? raw2.slice(1, -1) : raw2;
      const prefix = !pathPart && rRel.test(cmd) ? cmd.slice(0, 2) : "";
      return prefix + join3(pathPart, cmd);
    };
    var which3 = async (cmd, opt = {}) => {
      const { pathEnv, pathExt, pathExtExe } = getPathInfo(cmd, opt);
      const found = [];
      for (const envPart of pathEnv) {
        const p = getPathPart(envPart, cmd);
        for (const ext of pathExt) {
          const withExt = p + ext;
          const is = await isexe(withExt, { pathExt: pathExtExe, ignoreErrors: true });
          if (is) {
            if (!opt.all) {
              return withExt;
            }
            found.push(withExt);
          }
        }
      }
      if (opt.all && found.length) {
        return found;
      }
      if (opt.nothrow) {
        return null;
      }
      throw getNotFoundError(cmd);
    };
    var whichSync = (cmd, opt = {}) => {
      const { pathEnv, pathExt, pathExtExe } = getPathInfo(cmd, opt);
      const found = [];
      for (const pathEnvPart of pathEnv) {
        const p = getPathPart(pathEnvPart, cmd);
        for (const ext of pathExt) {
          const withExt = p + ext;
          const is = isexeSync(withExt, { pathExt: pathExtExe, ignoreErrors: true });
          if (is) {
            if (!opt.all) {
              return withExt;
            }
            found.push(withExt);
          }
        }
      }
      if (opt.all && found.length) {
        return found;
      }
      if (opt.nothrow) {
        return null;
      }
      throw getNotFoundError(cmd);
    };
    module2.exports = which3;
    which3.sync = whichSync;
  }
});

// node_modules/is-windows/index.js
var require_is_windows = __commonJS({
  "node_modules/is-windows/index.js"(exports2, module2) {
    (function(factory) {
      if (exports2 && typeof exports2 === "object" && typeof module2 !== "undefined") {
        module2.exports = factory();
      } else if (typeof define === "function" && define.amd) {
        define([], factory);
      } else if (typeof window !== "undefined") {
        window.isWindows = factory();
      } else if (typeof global !== "undefined") {
        global.isWindows = factory();
      } else if (typeof self !== "undefined") {
        self.isWindows = factory();
      } else {
        this.isWindows = factory();
      }
    })(function() {
      "use strict";
      return function isWindows4() {
        return process && (process.platform === "win32" || /^(msys|cygwin)$/.test(process.env.OSTYPE));
      };
    });
  }
});

// node_modules/cmd-extension/index.js
var require_cmd_extension = __commonJS({
  "node_modules/cmd-extension/index.js"(exports2, module2) {
    "use strict";
    var path16 = require("path");
    var cmdExtension;
    if (process.env.PATHEXT) {
      cmdExtension = process.env.PATHEXT.split(path16.delimiter).find((ext) => ext.toUpperCase() === ".CMD");
    }
    module2.exports = cmdExtension || ".cmd";
  }
});

// node_modules/graceful-fs/polyfills.js
var require_polyfills = __commonJS({
  "node_modules/graceful-fs/polyfills.js"(exports2, module2) {
    var constants2 = require("constants");
    var origCwd = process.cwd;
    var cwd = null;
    var platform5 = process.env.GRACEFUL_FS_PLATFORM || process.platform;
    process.cwd = function() {
      if (!cwd)
        cwd = origCwd.call(process);
      return cwd;
    };
    try {
      process.cwd();
    } catch (er) {
    }
    if (typeof process.chdir === "function") {
      chdir = process.chdir;
      process.chdir = function(d) {
        cwd = null;
        chdir.call(process, d);
      };
      if (Object.setPrototypeOf) Object.setPrototypeOf(process.chdir, chdir);
    }
    var chdir;
    module2.exports = patch;
    function patch(fs17) {
      if (constants2.hasOwnProperty("O_SYMLINK") && process.version.match(/^v0\.6\.[0-2]|^v0\.5\./)) {
        patchLchmod(fs17);
      }
      if (!fs17.lutimes) {
        patchLutimes(fs17);
      }
      fs17.chown = chownFix(fs17.chown);
      fs17.fchown = chownFix(fs17.fchown);
      fs17.lchown = chownFix(fs17.lchown);
      fs17.chmod = chmodFix(fs17.chmod);
      fs17.fchmod = chmodFix(fs17.fchmod);
      fs17.lchmod = chmodFix(fs17.lchmod);
      fs17.chownSync = chownFixSync(fs17.chownSync);
      fs17.fchownSync = chownFixSync(fs17.fchownSync);
      fs17.lchownSync = chownFixSync(fs17.lchownSync);
      fs17.chmodSync = chmodFixSync(fs17.chmodSync);
      fs17.fchmodSync = chmodFixSync(fs17.fchmodSync);
      fs17.lchmodSync = chmodFixSync(fs17.lchmodSync);
      fs17.stat = statFix(fs17.stat);
      fs17.fstat = statFix(fs17.fstat);
      fs17.lstat = statFix(fs17.lstat);
      fs17.statSync = statFixSync(fs17.statSync);
      fs17.fstatSync = statFixSync(fs17.fstatSync);
      fs17.lstatSync = statFixSync(fs17.lstatSync);
      if (fs17.chmod && !fs17.lchmod) {
        fs17.lchmod = function(path16, mode, cb) {
          if (cb) process.nextTick(cb);
        };
        fs17.lchmodSync = function() {
        };
      }
      if (fs17.chown && !fs17.lchown) {
        fs17.lchown = function(path16, uid, gid, cb) {
          if (cb) process.nextTick(cb);
        };
        fs17.lchownSync = function() {
        };
      }
      if (platform5 === "win32") {
        fs17.rename = typeof fs17.rename !== "function" ? fs17.rename : (function(fs$rename) {
          function rename(from, to, cb) {
            var start = Date.now();
            var backoff = 0;
            fs$rename(from, to, function CB(er) {
              if (er && (er.code === "EACCES" || er.code === "EPERM" || er.code === "EBUSY") && Date.now() - start < 6e4) {
                setTimeout(function() {
                  fs17.stat(to, function(stater, st) {
                    if (stater && stater.code === "ENOENT")
                      fs$rename(from, to, CB);
                    else
                      cb(er);
                  });
                }, backoff);
                if (backoff < 100)
                  backoff += 10;
                return;
              }
              if (cb) cb(er);
            });
          }
          if (Object.setPrototypeOf) Object.setPrototypeOf(rename, fs$rename);
          return rename;
        })(fs17.rename);
      }
      fs17.read = typeof fs17.read !== "function" ? fs17.read : (function(fs$read) {
        function read(fd, buffer, offset, length, position, callback_) {
          var callback;
          if (callback_ && typeof callback_ === "function") {
            var eagCounter = 0;
            callback = function(er, _, __) {
              if (er && er.code === "EAGAIN" && eagCounter < 10) {
                eagCounter++;
                return fs$read.call(fs17, fd, buffer, offset, length, position, callback);
              }
              callback_.apply(this, arguments);
            };
          }
          return fs$read.call(fs17, fd, buffer, offset, length, position, callback);
        }
        if (Object.setPrototypeOf) Object.setPrototypeOf(read, fs$read);
        return read;
      })(fs17.read);
      fs17.readSync = typeof fs17.readSync !== "function" ? fs17.readSync : /* @__PURE__ */ (function(fs$readSync) {
        return function(fd, buffer, offset, length, position) {
          var eagCounter = 0;
          while (true) {
            try {
              return fs$readSync.call(fs17, fd, buffer, offset, length, position);
            } catch (er) {
              if (er.code === "EAGAIN" && eagCounter < 10) {
                eagCounter++;
                continue;
              }
              throw er;
            }
          }
        };
      })(fs17.readSync);
      function patchLchmod(fs18) {
        fs18.lchmod = function(path16, mode, callback) {
          fs18.open(
            path16,
            constants2.O_WRONLY | constants2.O_SYMLINK,
            mode,
            function(err, fd) {
              if (err) {
                if (callback) callback(err);
                return;
              }
              fs18.fchmod(fd, mode, function(err2) {
                fs18.close(fd, function(err22) {
                  if (callback) callback(err2 || err22);
                });
              });
            }
          );
        };
        fs18.lchmodSync = function(path16, mode) {
          var fd = fs18.openSync(path16, constants2.O_WRONLY | constants2.O_SYMLINK, mode);
          var threw = true;
          var ret;
          try {
            ret = fs18.fchmodSync(fd, mode);
            threw = false;
          } finally {
            if (threw) {
              try {
                fs18.closeSync(fd);
              } catch (er) {
              }
            } else {
              fs18.closeSync(fd);
            }
          }
          return ret;
        };
      }
      function patchLutimes(fs18) {
        if (constants2.hasOwnProperty("O_SYMLINK") && fs18.futimes) {
          fs18.lutimes = function(path16, at, mt, cb) {
            fs18.open(path16, constants2.O_SYMLINK, function(er, fd) {
              if (er) {
                if (cb) cb(er);
                return;
              }
              fs18.futimes(fd, at, mt, function(er2) {
                fs18.close(fd, function(er22) {
                  if (cb) cb(er2 || er22);
                });
              });
            });
          };
          fs18.lutimesSync = function(path16, at, mt) {
            var fd = fs18.openSync(path16, constants2.O_SYMLINK);
            var ret;
            var threw = true;
            try {
              ret = fs18.futimesSync(fd, at, mt);
              threw = false;
            } finally {
              if (threw) {
                try {
                  fs18.closeSync(fd);
                } catch (er) {
                }
              } else {
                fs18.closeSync(fd);
              }
            }
            return ret;
          };
        } else if (fs18.futimes) {
          fs18.lutimes = function(_a, _b, _c, cb) {
            if (cb) process.nextTick(cb);
          };
          fs18.lutimesSync = function() {
          };
        }
      }
      function chmodFix(orig) {
        if (!orig) return orig;
        return function(target, mode, cb) {
          return orig.call(fs17, target, mode, function(er) {
            if (chownErOk(er)) er = null;
            if (cb) cb.apply(this, arguments);
          });
        };
      }
      function chmodFixSync(orig) {
        if (!orig) return orig;
        return function(target, mode) {
          try {
            return orig.call(fs17, target, mode);
          } catch (er) {
            if (!chownErOk(er)) throw er;
          }
        };
      }
      function chownFix(orig) {
        if (!orig) return orig;
        return function(target, uid, gid, cb) {
          return orig.call(fs17, target, uid, gid, function(er) {
            if (chownErOk(er)) er = null;
            if (cb) cb.apply(this, arguments);
          });
        };
      }
      function chownFixSync(orig) {
        if (!orig) return orig;
        return function(target, uid, gid) {
          try {
            return orig.call(fs17, target, uid, gid);
          } catch (er) {
            if (!chownErOk(er)) throw er;
          }
        };
      }
      function statFix(orig) {
        if (!orig) return orig;
        return function(target, options, cb) {
          if (typeof options === "function") {
            cb = options;
            options = null;
          }
          function callback(er, stats) {
            if (stats) {
              if (stats.uid < 0) stats.uid += 4294967296;
              if (stats.gid < 0) stats.gid += 4294967296;
            }
            if (cb) cb.apply(this, arguments);
          }
          return options ? orig.call(fs17, target, options, callback) : orig.call(fs17, target, callback);
        };
      }
      function statFixSync(orig) {
        if (!orig) return orig;
        return function(target, options) {
          var stats = options ? orig.call(fs17, target, options) : orig.call(fs17, target);
          if (stats) {
            if (stats.uid < 0) stats.uid += 4294967296;
            if (stats.gid < 0) stats.gid += 4294967296;
          }
          return stats;
        };
      }
      function chownErOk(er) {
        if (!er)
          return true;
        if (er.code === "ENOSYS")
          return true;
        var nonroot = !process.getuid || process.getuid() !== 0;
        if (nonroot) {
          if (er.code === "EINVAL" || er.code === "EPERM")
            return true;
        }
        return false;
      }
    }
  }
});

// node_modules/graceful-fs/legacy-streams.js
var require_legacy_streams = __commonJS({
  "node_modules/graceful-fs/legacy-streams.js"(exports2, module2) {
    var Stream2 = require("stream").Stream;
    module2.exports = legacy;
    function legacy(fs17) {
      return {
        ReadStream: ReadStream2,
        WriteStream: WriteStream2
      };
      function ReadStream2(path16, options) {
        if (!(this instanceof ReadStream2)) return new ReadStream2(path16, options);
        Stream2.call(this);
        var self2 = this;
        this.path = path16;
        this.fd = null;
        this.readable = true;
        this.paused = false;
        this.flags = "r";
        this.mode = 438;
        this.bufferSize = 64 * 1024;
        options = options || {};
        var keys = Object.keys(options);
        for (var index = 0, length = keys.length; index < length; index++) {
          var key = keys[index];
          this[key] = options[key];
        }
        if (this.encoding) this.setEncoding(this.encoding);
        if (this.start !== void 0) {
          if ("number" !== typeof this.start) {
            throw TypeError("start must be a Number");
          }
          if (this.end === void 0) {
            this.end = Infinity;
          } else if ("number" !== typeof this.end) {
            throw TypeError("end must be a Number");
          }
          if (this.start > this.end) {
            throw new Error("start must be <= end");
          }
          this.pos = this.start;
        }
        if (this.fd !== null) {
          process.nextTick(function() {
            self2._read();
          });
          return;
        }
        fs17.open(this.path, this.flags, this.mode, function(err, fd) {
          if (err) {
            self2.emit("error", err);
            self2.readable = false;
            return;
          }
          self2.fd = fd;
          self2.emit("open", fd);
          self2._read();
        });
      }
      function WriteStream2(path16, options) {
        if (!(this instanceof WriteStream2)) return new WriteStream2(path16, options);
        Stream2.call(this);
        this.path = path16;
        this.fd = null;
        this.writable = true;
        this.flags = "w";
        this.encoding = "binary";
        this.mode = 438;
        this.bytesWritten = 0;
        options = options || {};
        var keys = Object.keys(options);
        for (var index = 0, length = keys.length; index < length; index++) {
          var key = keys[index];
          this[key] = options[key];
        }
        if (this.start !== void 0) {
          if ("number" !== typeof this.start) {
            throw TypeError("start must be a Number");
          }
          if (this.start < 0) {
            throw new Error("start must be >= zero");
          }
          this.pos = this.start;
        }
        this.busy = false;
        this._queue = [];
        if (this.fd === null) {
          this._open = fs17.open;
          this._queue.push([this._open, this.path, this.flags, this.mode, void 0]);
          this.flush();
        }
      }
    }
  }
});

// node_modules/graceful-fs/clone.js
var require_clone = __commonJS({
  "node_modules/graceful-fs/clone.js"(exports2, module2) {
    "use strict";
    module2.exports = clone;
    var getPrototypeOf = Object.getPrototypeOf || function(obj) {
      return obj.__proto__;
    };
    function clone(obj) {
      if (obj === null || typeof obj !== "object")
        return obj;
      if (obj instanceof Object)
        var copy = { __proto__: getPrototypeOf(obj) };
      else
        var copy = /* @__PURE__ */ Object.create(null);
      Object.getOwnPropertyNames(obj).forEach(function(key) {
        Object.defineProperty(copy, key, Object.getOwnPropertyDescriptor(obj, key));
      });
      return copy;
    }
  }
});

// node_modules/graceful-fs/graceful-fs.js
var require_graceful_fs = __commonJS({
  "node_modules/graceful-fs/graceful-fs.js"(exports2, module2) {
    var fs17 = require("fs");
    var polyfills = require_polyfills();
    var legacy = require_legacy_streams();
    var clone = require_clone();
    var util = require("util");
    var gracefulQueue;
    var previousSymbol;
    if (typeof Symbol === "function" && typeof Symbol.for === "function") {
      gracefulQueue = /* @__PURE__ */ Symbol.for("graceful-fs.queue");
      previousSymbol = /* @__PURE__ */ Symbol.for("graceful-fs.previous");
    } else {
      gracefulQueue = "___graceful-fs.queue";
      previousSymbol = "___graceful-fs.previous";
    }
    function noop3() {
    }
    function publishQueue(context, queue2) {
      Object.defineProperty(context, gracefulQueue, {
        get: function() {
          return queue2;
        }
      });
    }
    var debug = noop3;
    if (util.debuglog)
      debug = util.debuglog("gfs4");
    else if (/\bgfs4\b/i.test(process.env.NODE_DEBUG || ""))
      debug = function() {
        var m = util.format.apply(util, arguments);
        m = "GFS4: " + m.split(/\n/).join("\nGFS4: ");
        console.error(m);
      };
    if (!fs17[gracefulQueue]) {
      queue = global[gracefulQueue] || [];
      publishQueue(fs17, queue);
      fs17.close = (function(fs$close) {
        function close(fd, cb) {
          return fs$close.call(fs17, fd, function(err) {
            if (!err) {
              resetQueue();
            }
            if (typeof cb === "function")
              cb.apply(this, arguments);
          });
        }
        Object.defineProperty(close, previousSymbol, {
          value: fs$close
        });
        return close;
      })(fs17.close);
      fs17.closeSync = (function(fs$closeSync) {
        function closeSync(fd) {
          fs$closeSync.apply(fs17, arguments);
          resetQueue();
        }
        Object.defineProperty(closeSync, previousSymbol, {
          value: fs$closeSync
        });
        return closeSync;
      })(fs17.closeSync);
      if (/\bgfs4\b/i.test(process.env.NODE_DEBUG || "")) {
        process.on("exit", function() {
          debug(fs17[gracefulQueue]);
          require("assert").equal(fs17[gracefulQueue].length, 0);
        });
      }
    }
    var queue;
    if (!global[gracefulQueue]) {
      publishQueue(global, fs17[gracefulQueue]);
    }
    module2.exports = patch(clone(fs17));
    if (process.env.TEST_GRACEFUL_FS_GLOBAL_PATCH && !fs17.__patched) {
      module2.exports = patch(fs17);
      fs17.__patched = true;
    }
    function patch(fs18) {
      polyfills(fs18);
      fs18.gracefulify = patch;
      fs18.createReadStream = createReadStream;
      fs18.createWriteStream = createWriteStream;
      var fs$readFile = fs18.readFile;
      fs18.readFile = readFile;
      function readFile(path16, options, cb) {
        if (typeof options === "function")
          cb = options, options = null;
        return go$readFile(path16, options, cb);
        function go$readFile(path17, options2, cb2, startTime) {
          return fs$readFile(path17, options2, function(err) {
            if (err && (err.code === "EMFILE" || err.code === "ENFILE"))
              enqueue([go$readFile, [path17, options2, cb2], err, startTime || Date.now(), Date.now()]);
            else {
              if (typeof cb2 === "function")
                cb2.apply(this, arguments);
            }
          });
        }
      }
      var fs$writeFile = fs18.writeFile;
      fs18.writeFile = writeFile;
      function writeFile(path16, data, options, cb) {
        if (typeof options === "function")
          cb = options, options = null;
        return go$writeFile(path16, data, options, cb);
        function go$writeFile(path17, data2, options2, cb2, startTime) {
          return fs$writeFile(path17, data2, options2, function(err) {
            if (err && (err.code === "EMFILE" || err.code === "ENFILE"))
              enqueue([go$writeFile, [path17, data2, options2, cb2], err, startTime || Date.now(), Date.now()]);
            else {
              if (typeof cb2 === "function")
                cb2.apply(this, arguments);
            }
          });
        }
      }
      var fs$appendFile = fs18.appendFile;
      if (fs$appendFile)
        fs18.appendFile = appendFile;
      function appendFile(path16, data, options, cb) {
        if (typeof options === "function")
          cb = options, options = null;
        return go$appendFile(path16, data, options, cb);
        function go$appendFile(path17, data2, options2, cb2, startTime) {
          return fs$appendFile(path17, data2, options2, function(err) {
            if (err && (err.code === "EMFILE" || err.code === "ENFILE"))
              enqueue([go$appendFile, [path17, data2, options2, cb2], err, startTime || Date.now(), Date.now()]);
            else {
              if (typeof cb2 === "function")
                cb2.apply(this, arguments);
            }
          });
        }
      }
      var fs$copyFile = fs18.copyFile;
      if (fs$copyFile)
        fs18.copyFile = copyFile;
      function copyFile(src, dest, flags, cb) {
        if (typeof flags === "function") {
          cb = flags;
          flags = 0;
        }
        return go$copyFile(src, dest, flags, cb);
        function go$copyFile(src2, dest2, flags2, cb2, startTime) {
          return fs$copyFile(src2, dest2, flags2, function(err) {
            if (err && (err.code === "EMFILE" || err.code === "ENFILE"))
              enqueue([go$copyFile, [src2, dest2, flags2, cb2], err, startTime || Date.now(), Date.now()]);
            else {
              if (typeof cb2 === "function")
                cb2.apply(this, arguments);
            }
          });
        }
      }
      var fs$readdir = fs18.readdir;
      fs18.readdir = readdir;
      var noReaddirOptionVersions = /^v[0-5]\./;
      function readdir(path16, options, cb) {
        if (typeof options === "function")
          cb = options, options = null;
        var go$readdir = noReaddirOptionVersions.test(process.version) ? function go$readdir2(path17, options2, cb2, startTime) {
          return fs$readdir(path17, fs$readdirCallback(
            path17,
            options2,
            cb2,
            startTime
          ));
        } : function go$readdir2(path17, options2, cb2, startTime) {
          return fs$readdir(path17, options2, fs$readdirCallback(
            path17,
            options2,
            cb2,
            startTime
          ));
        };
        return go$readdir(path16, options, cb);
        function fs$readdirCallback(path17, options2, cb2, startTime) {
          return function(err, files) {
            if (err && (err.code === "EMFILE" || err.code === "ENFILE"))
              enqueue([
                go$readdir,
                [path17, options2, cb2],
                err,
                startTime || Date.now(),
                Date.now()
              ]);
            else {
              if (files && files.sort)
                files.sort();
              if (typeof cb2 === "function")
                cb2.call(this, err, files);
            }
          };
        }
      }
      if (process.version.substr(0, 4) === "v0.8") {
        var legStreams = legacy(fs18);
        ReadStream2 = legStreams.ReadStream;
        WriteStream2 = legStreams.WriteStream;
      }
      var fs$ReadStream = fs18.ReadStream;
      if (fs$ReadStream) {
        ReadStream2.prototype = Object.create(fs$ReadStream.prototype);
        ReadStream2.prototype.open = ReadStream$open;
      }
      var fs$WriteStream = fs18.WriteStream;
      if (fs$WriteStream) {
        WriteStream2.prototype = Object.create(fs$WriteStream.prototype);
        WriteStream2.prototype.open = WriteStream$open;
      }
      Object.defineProperty(fs18, "ReadStream", {
        get: function() {
          return ReadStream2;
        },
        set: function(val) {
          ReadStream2 = val;
        },
        enumerable: true,
        configurable: true
      });
      Object.defineProperty(fs18, "WriteStream", {
        get: function() {
          return WriteStream2;
        },
        set: function(val) {
          WriteStream2 = val;
        },
        enumerable: true,
        configurable: true
      });
      var FileReadStream = ReadStream2;
      Object.defineProperty(fs18, "FileReadStream", {
        get: function() {
          return FileReadStream;
        },
        set: function(val) {
          FileReadStream = val;
        },
        enumerable: true,
        configurable: true
      });
      var FileWriteStream = WriteStream2;
      Object.defineProperty(fs18, "FileWriteStream", {
        get: function() {
          return FileWriteStream;
        },
        set: function(val) {
          FileWriteStream = val;
        },
        enumerable: true,
        configurable: true
      });
      function ReadStream2(path16, options) {
        if (this instanceof ReadStream2)
          return fs$ReadStream.apply(this, arguments), this;
        else
          return ReadStream2.apply(Object.create(ReadStream2.prototype), arguments);
      }
      function ReadStream$open() {
        var that = this;
        open(that.path, that.flags, that.mode, function(err, fd) {
          if (err) {
            if (that.autoClose)
              that.destroy();
            that.emit("error", err);
          } else {
            that.fd = fd;
            that.emit("open", fd);
            that.read();
          }
        });
      }
      function WriteStream2(path16, options) {
        if (this instanceof WriteStream2)
          return fs$WriteStream.apply(this, arguments), this;
        else
          return WriteStream2.apply(Object.create(WriteStream2.prototype), arguments);
      }
      function WriteStream$open() {
        var that = this;
        open(that.path, that.flags, that.mode, function(err, fd) {
          if (err) {
            that.destroy();
            that.emit("error", err);
          } else {
            that.fd = fd;
            that.emit("open", fd);
          }
        });
      }
      function createReadStream(path16, options) {
        return new fs18.ReadStream(path16, options);
      }
      function createWriteStream(path16, options) {
        return new fs18.WriteStream(path16, options);
      }
      var fs$open = fs18.open;
      fs18.open = open;
      function open(path16, flags, mode, cb) {
        if (typeof mode === "function")
          cb = mode, mode = null;
        return go$open(path16, flags, mode, cb);
        function go$open(path17, flags2, mode2, cb2, startTime) {
          return fs$open(path17, flags2, mode2, function(err, fd) {
            if (err && (err.code === "EMFILE" || err.code === "ENFILE"))
              enqueue([go$open, [path17, flags2, mode2, cb2], err, startTime || Date.now(), Date.now()]);
            else {
              if (typeof cb2 === "function")
                cb2.apply(this, arguments);
            }
          });
        }
      }
      return fs18;
    }
    function enqueue(elem) {
      debug("ENQUEUE", elem[0].name, elem[1]);
      fs17[gracefulQueue].push(elem);
      retry();
    }
    var retryTimer;
    function resetQueue() {
      var now = Date.now();
      for (var i = 0; i < fs17[gracefulQueue].length; ++i) {
        if (fs17[gracefulQueue][i].length > 2) {
          fs17[gracefulQueue][i][3] = now;
          fs17[gracefulQueue][i][4] = now;
        }
      }
      retry();
    }
    function retry() {
      clearTimeout(retryTimer);
      retryTimer = void 0;
      if (fs17[gracefulQueue].length === 0)
        return;
      var elem = fs17[gracefulQueue].shift();
      var fn2 = elem[0];
      var args = elem[1];
      var err = elem[2];
      var startTime = elem[3];
      var lastTime = elem[4];
      if (startTime === void 0) {
        debug("RETRY", fn2.name, args);
        fn2.apply(null, args);
      } else if (Date.now() - startTime >= 6e4) {
        debug("TIMEOUT", fn2.name, args);
        var cb = args.pop();
        if (typeof cb === "function")
          cb.call(null, err);
      } else {
        var sinceAttempt = Date.now() - lastTime;
        var sinceStart = Math.max(lastTime - startTime, 1);
        var desiredDelay = Math.min(sinceStart * 1.2, 100);
        if (sinceAttempt >= desiredDelay) {
          debug("RETRY", fn2.name, args);
          fn2.apply(null, args.concat([startTime]));
        } else {
          fs17[gracefulQueue].push(elem);
        }
      }
      if (retryTimer === void 0) {
        retryTimer = setTimeout(retry, 0);
      }
    }
  }
});

// node_modules/@zkochan/cmd-shim/index.js
var require_cmd_shim = __commonJS({
  "node_modules/@zkochan/cmd-shim/index.js"(exports2, module2) {
    "use strict";
    cmdShim2.ifExists = cmdShimIfExists;
    var util_1 = require("util");
    var path16 = require("path");
    var isWindows4 = require_is_windows();
    var CMD_EXTENSION = require_cmd_extension();
    var shebangExpr = /^#!\s*(?:\/usr\/bin\/env(?:\s+-S\s*)?)?\s*([^ \t]+)(.*)$/;
    var DEFAULT_OPTIONS = {
      // Create PowerShell file by default if the option hasn't been specified
      createPwshFile: true,
      createCmdFile: isWindows4(),
      fs: require_graceful_fs()
    };
    var extensionToProgramMap = /* @__PURE__ */ new Map([
      [".js", "node"],
      [".cjs", "node"],
      [".mjs", "node"],
      [".cmd", "cmd"],
      [".bat", "cmd"],
      [".ps1", "pwsh"],
      [".sh", "sh"]
    ]);
    function ingestOptions(opts) {
      const opts_ = { ...DEFAULT_OPTIONS, ...opts };
      const fs17 = opts_.fs;
      opts_.fs_ = {
        chmod: fs17.chmod ? (0, util_1.promisify)(fs17.chmod) : (async () => {
        }),
        mkdir: (0, util_1.promisify)(fs17.mkdir),
        readFile: (0, util_1.promisify)(fs17.readFile),
        stat: (0, util_1.promisify)(fs17.stat),
        unlink: (0, util_1.promisify)(fs17.unlink),
        writeFile: (0, util_1.promisify)(fs17.writeFile)
      };
      return opts_;
    }
    async function cmdShim2(src, to, opts) {
      const opts_ = ingestOptions(opts);
      await cmdShim_(src, to, opts_);
    }
    function cmdShimIfExists(src, to, opts) {
      return cmdShim2(src, to, opts).catch(() => {
      });
    }
    function rm(path17, opts) {
      return opts.fs_.unlink(path17).catch(() => {
      });
    }
    async function cmdShim_(src, to, opts) {
      const srcRuntimeInfo = await searchScriptRuntime(src, opts);
      await writeShimsPreCommon(to, opts);
      return writeAllShims(src, to, srcRuntimeInfo, opts);
    }
    function writeShimsPreCommon(target, opts) {
      return opts.fs_.mkdir(path16.dirname(target), { recursive: true });
    }
    function writeAllShims(src, to, srcRuntimeInfo, opts) {
      const opts_ = ingestOptions(opts);
      const generatorAndExts = [{ generator: generateShShim, extension: "" }];
      if (opts_.createCmdFile) {
        generatorAndExts.push({ generator: generateCmdShim, extension: CMD_EXTENSION });
      }
      if (opts_.createPwshFile) {
        generatorAndExts.push({ generator: generatePwshShim, extension: ".ps1" });
      }
      return Promise.all(generatorAndExts.map((generatorAndExt) => writeShim(src, to + generatorAndExt.extension, srcRuntimeInfo, generatorAndExt.generator, opts_)));
    }
    function writeShimPre(target, opts) {
      return rm(target, opts);
    }
    function writeShimPost(target, opts) {
      return chmodShim(target, opts);
    }
    async function searchScriptRuntime(target, opts) {
      try {
        const data = await opts.fs_.readFile(target, "utf8");
        const firstLine = data.trim().split(/\r*\n/)[0];
        const shebang = firstLine.match(shebangExpr);
        if (!shebang) {
          const targetExtension = path16.extname(target).toLowerCase();
          return {
            // undefined if extension is unknown but it's converted to null.
            program: extensionToProgramMap.get(targetExtension) || null,
            additionalArgs: ""
          };
        }
        return {
          program: shebang[1],
          additionalArgs: shebang[2]
        };
      } catch (err) {
        if (!isWindows4() || err.code !== "ENOENT")
          throw err;
        if (await opts.fs_.stat(`${target}${getExeExtension()}`)) {
          return {
            program: null,
            additionalArgs: ""
          };
        }
        throw err;
      }
    }
    function getExeExtension() {
      let cmdExtension;
      if (process.env.PATHEXT) {
        cmdExtension = process.env.PATHEXT.split(path16.delimiter).find((ext) => ext.toLowerCase() === ".exe");
      }
      return cmdExtension || ".exe";
    }
    async function writeShim(src, to, srcRuntimeInfo, generateShimScript, opts) {
      const defaultArgs = opts.preserveSymlinks ? "--preserve-symlinks" : "";
      const args = [srcRuntimeInfo.additionalArgs, defaultArgs].filter((arg) => arg).join(" ");
      opts = Object.assign({}, opts, {
        prog: srcRuntimeInfo.program,
        args
      });
      await writeShimPre(to, opts);
      await opts.fs_.writeFile(to, generateShimScript(src, to, opts), "utf8");
      return writeShimPost(to, opts);
    }
    function generateCmdShim(src, to, opts) {
      const shTarget = path16.relative(path16.dirname(to), src);
      let target = shTarget.split("/").join("\\");
      const quotedPathToTarget = path16.isAbsolute(target) ? `"${target}"` : `"%~dp0\\${target}"`;
      let longProg;
      let prog = opts.prog;
      let args = opts.args || "";
      const nodePath = normalizePathEnvVar(opts.nodePath).win32;
      const prependToPath = normalizePathEnvVar(opts.prependToPath).win32;
      if (!prog) {
        prog = quotedPathToTarget;
        args = "";
        target = "";
      } else if (prog === "node" && opts.nodeExecPath) {
        prog = `"${opts.nodeExecPath}"`;
        target = quotedPathToTarget;
      } else {
        longProg = `"%~dp0\\${prog}.exe"`;
        target = quotedPathToTarget;
      }
      let progArgs = opts.progArgs ? `${opts.progArgs.join(` `)} ` : "";
      let cmd = "@SETLOCAL\r\n";
      if (prependToPath) {
        cmd += `@SET "PATH=${prependToPath}:%PATH%"\r
`;
      }
      if (nodePath) {
        cmd += `@IF NOT DEFINED NODE_PATH (\r
  @SET "NODE_PATH=${nodePath}"\r
) ELSE (\r
  @SET "NODE_PATH=${nodePath};%NODE_PATH%"\r
)\r
`;
      }
      if (longProg) {
        cmd += `@IF EXIST ${longProg} (\r
  ${longProg} ${args} ${target} ${progArgs}%*\r
) ELSE (\r
  @SET PATHEXT=%PATHEXT:;.JS;=;%\r
  ${prog} ${args} ${target} ${progArgs}%*\r
)\r
`;
      } else {
        cmd += `@${prog} ${args} ${target} ${progArgs}%*\r
`;
      }
      return cmd;
    }
    function generateShShim(src, to, opts) {
      let shTarget = path16.relative(path16.dirname(to), src);
      let shProg = opts.prog && opts.prog.split("\\").join("/");
      let shLongProg;
      shTarget = shTarget.split("\\").join("/");
      const quotedPathToTarget = path16.isAbsolute(shTarget) ? `"${shTarget}"` : `"$basedir/${shTarget}"`;
      let args = opts.args || "";
      const shNodePath = normalizePathEnvVar(opts.nodePath).posix;
      if (!shProg) {
        shProg = quotedPathToTarget;
        args = "";
        shTarget = "";
      } else if (opts.prog === "node" && opts.nodeExecPath) {
        shProg = `"${opts.nodeExecPath}"`;
        shTarget = quotedPathToTarget;
      } else {
        shLongProg = `"$basedir/${opts.prog}"`;
        shTarget = quotedPathToTarget;
      }
      let progArgs = opts.progArgs ? `${opts.progArgs.join(` `)} ` : "";
      let sh = `#!/bin/sh
basedir=$(dirname "$(echo "$0" | sed -e 's,\\\\,/,g')")

case \`uname\` in
    *CYGWIN*) basedir=\`cygpath -w "$basedir"\`;;
esac

`;
      if (opts.prependToPath) {
        sh += `export PATH="${opts.prependToPath}:$PATH"
`;
      }
      if (shNodePath) {
        sh += `if [ -z "$NODE_PATH" ]; then
  export NODE_PATH="${shNodePath}"
else
  export NODE_PATH="${shNodePath}:$NODE_PATH"
fi
`;
      }
      if (shLongProg) {
        sh += `if [ -x ${shLongProg} ]; then
  exec ${shLongProg} ${args} ${shTarget} ${progArgs}"$@"
else
  exec ${shProg} ${args} ${shTarget} ${progArgs}"$@"
fi
`;
      } else {
        sh += `${shProg} ${args} ${shTarget} ${progArgs}"$@"
exit $?
`;
      }
      return sh;
    }
    function generatePwshShim(src, to, opts) {
      let shTarget = path16.relative(path16.dirname(to), src);
      const shProg = opts.prog && opts.prog.split("\\").join("/");
      let pwshProg = shProg && `"${shProg}$exe"`;
      let pwshLongProg;
      shTarget = shTarget.split("\\").join("/");
      const quotedPathToTarget = path16.isAbsolute(shTarget) ? `"${shTarget}"` : `"$basedir/${shTarget}"`;
      let args = opts.args || "";
      let normalizedNodePathEnvVar = normalizePathEnvVar(opts.nodePath);
      const nodePath = normalizedNodePathEnvVar.win32;
      const shNodePath = normalizedNodePathEnvVar.posix;
      let normalizedPrependPathEnvVar = normalizePathEnvVar(opts.prependToPath);
      const prependPath = normalizedPrependPathEnvVar.win32;
      const shPrependPath = normalizedPrependPathEnvVar.posix;
      if (!pwshProg) {
        pwshProg = quotedPathToTarget;
        args = "";
        shTarget = "";
      } else if (opts.prog === "node" && opts.nodeExecPath) {
        pwshProg = `"${opts.nodeExecPath}"`;
        shTarget = quotedPathToTarget;
      } else {
        pwshLongProg = `"$basedir/${opts.prog}$exe"`;
        shTarget = quotedPathToTarget;
      }
      let progArgs = opts.progArgs ? `${opts.progArgs.join(` `)} ` : "";
      let pwsh = `#!/usr/bin/env pwsh
$basedir=Split-Path $MyInvocation.MyCommand.Definition -Parent

$exe=""
${nodePath || prependPath ? '$pathsep=":"\n' : ""}${nodePath ? `$env_node_path=$env:NODE_PATH
$new_node_path="${nodePath}"
` : ""}${prependPath ? `$env_path=$env:PATH
$prepend_path="${prependPath}"
` : ""}if ($PSVersionTable.PSVersion -lt "6.0" -or $IsWindows) {
  # Fix case when both the Windows and Linux builds of Node
  # are installed in the same directory
  $exe=".exe"
${nodePath || prependPath ? '  $pathsep=";"\n' : ""}}`;
      if (shNodePath || shPrependPath) {
        pwsh += ` else {
${shNodePath ? `  $new_node_path="${shNodePath}"
` : ""}${shPrependPath ? `  $prepend_path="${shPrependPath}"
` : ""}}
`;
      }
      if (shNodePath) {
        pwsh += `if ([string]::IsNullOrEmpty($env_node_path)) {
  $env:NODE_PATH=$new_node_path
} else {
  $env:NODE_PATH="$new_node_path$pathsep$env_node_path"
}
`;
      }
      if (opts.prependToPath) {
        pwsh += `
$env:PATH="$prepend_path$pathsep$env:PATH"
`;
      }
      if (pwshLongProg) {
        pwsh += `
$ret=0
if (Test-Path ${pwshLongProg}) {
  # Support pipeline input
  if ($MyInvocation.ExpectingInput) {
    $input | & ${pwshLongProg} ${args} ${shTarget} ${progArgs}$args
  } else {
    & ${pwshLongProg} ${args} ${shTarget} ${progArgs}$args
  }
  $ret=$LASTEXITCODE
} else {
  # Support pipeline input
  if ($MyInvocation.ExpectingInput) {
    $input | & ${pwshProg} ${args} ${shTarget} ${progArgs}$args
  } else {
    & ${pwshProg} ${args} ${shTarget} ${progArgs}$args
  }
  $ret=$LASTEXITCODE
}
${nodePath ? "$env:NODE_PATH=$env_node_path\n" : ""}${prependPath ? "$env:PATH=$env_path\n" : ""}exit $ret
`;
      } else {
        pwsh += `
# Support pipeline input
if ($MyInvocation.ExpectingInput) {
  $input | & ${pwshProg} ${args} ${shTarget} ${progArgs}$args
} else {
  & ${pwshProg} ${args} ${shTarget} ${progArgs}$args
}
${nodePath ? "$env:NODE_PATH=$env_node_path\n" : ""}${prependPath ? "$env:PATH=$env_path\n" : ""}exit $LASTEXITCODE
`;
      }
      return pwsh;
    }
    function chmodShim(to, opts) {
      return opts.fs_.chmod(to, 493);
    }
    function normalizePathEnvVar(nodePath) {
      if (!nodePath || !nodePath.length) {
        return {
          win32: "",
          posix: ""
        };
      }
      let split = typeof nodePath === "string" ? nodePath.split(path16.delimiter) : Array.from(nodePath);
      let result = {};
      for (let i = 0; i < split.length; i++) {
        const win322 = split[i].split("/").join("\\");
        const posix = isWindows4() ? split[i].split("\\").join("/").replace(/^([^:\\/]*):/, (_, $1) => `/mnt/${$1.toLowerCase()}`) : split[i];
        result.win32 = result.win32 ? `${result.win32};${win322}` : win322;
        result.posix = result.posix ? `${result.posix}:${posix}` : posix;
        result[i] = { win32: win322, posix };
      }
      return result;
    }
    module2.exports = cmdShim2;
  }
});

// node_modules/tar/dist/esm/mode-fix.js
var modeFix;
var init_mode_fix = __esm({
  "node_modules/tar/dist/esm/mode-fix.js"() {
    modeFix = (mode, isDir, portable) => {
      mode &= 4095;
      if (portable) {
        mode = (mode | 384) & ~18;
      }
      if (isDir) {
        if (mode & 256) {
          mode |= 64;
        }
        if (mode & 32) {
          mode |= 8;
        }
        if (mode & 4) {
          mode |= 1;
        }
      }
      return mode;
    };
  }
});

// node_modules/tar/dist/esm/write-entry.js
var import_fs11, import_path9, prefixPath, maxReadSize, PROCESS, FILE2, DIRECTORY2, SYMLINK2, HARDLINK2, HEADER, READ2, LSTAT, ONLSTAT, ONREAD, ONREADLINK, OPENFILE, ONOPENFILE, CLOSE, MODE, AWAITDRAIN, ONDRAIN, PREFIX, WriteEntry, WriteEntrySync, WriteEntryTar, getType;
var init_write_entry = __esm({
  "node_modules/tar/dist/esm/write-entry.js"() {
    import_fs11 = __toESM(require("fs"), 1);
    init_esm();
    import_path9 = __toESM(require("path"), 1);
    init_header();
    init_mode_fix();
    init_normalize_windows_path();
    init_options();
    init_pax();
    init_strip_absolute_path();
    init_strip_trailing_slashes();
    init_warn_method();
    init_winchars();
    prefixPath = (path16, prefix) => {
      if (!prefix) {
        return normalizeWindowsPath(path16);
      }
      path16 = normalizeWindowsPath(path16).replace(/^\.(\/|$)/, "");
      return stripTrailingSlashes(prefix) + "/" + path16;
    };
    maxReadSize = 16 * 1024 * 1024;
    PROCESS = /* @__PURE__ */ Symbol("process");
    FILE2 = /* @__PURE__ */ Symbol("file");
    DIRECTORY2 = /* @__PURE__ */ Symbol("directory");
    SYMLINK2 = /* @__PURE__ */ Symbol("symlink");
    HARDLINK2 = /* @__PURE__ */ Symbol("hardlink");
    HEADER = /* @__PURE__ */ Symbol("header");
    READ2 = /* @__PURE__ */ Symbol("read");
    LSTAT = /* @__PURE__ */ Symbol("lstat");
    ONLSTAT = /* @__PURE__ */ Symbol("onlstat");
    ONREAD = /* @__PURE__ */ Symbol("onread");
    ONREADLINK = /* @__PURE__ */ Symbol("onreadlink");
    OPENFILE = /* @__PURE__ */ Symbol("openfile");
    ONOPENFILE = /* @__PURE__ */ Symbol("onopenfile");
    CLOSE = /* @__PURE__ */ Symbol("close");
    MODE = /* @__PURE__ */ Symbol("mode");
    AWAITDRAIN = /* @__PURE__ */ Symbol("awaitDrain");
    ONDRAIN = /* @__PURE__ */ Symbol("ondrain");
    PREFIX = /* @__PURE__ */ Symbol("prefix");
    WriteEntry = class extends Minipass {
      path;
      portable;
      myuid = process.getuid && process.getuid() || 0;
      // until node has builtin pwnam functions, this'll have to do
      myuser = process.env.USER || "";
      maxReadSize;
      linkCache;
      statCache;
      preservePaths;
      cwd;
      strict;
      mtime;
      noPax;
      noMtime;
      prefix;
      fd;
      blockLen = 0;
      blockRemain = 0;
      buf;
      pos = 0;
      remain = 0;
      length = 0;
      offset = 0;
      win32;
      absolute;
      header;
      type;
      linkpath;
      stat;
      onWriteEntry;
      #hadError = false;
      constructor(p, opt_ = {}) {
        const opt = dealias(opt_);
        super();
        this.path = normalizeWindowsPath(p);
        this.portable = !!opt.portable;
        this.maxReadSize = opt.maxReadSize || maxReadSize;
        this.linkCache = opt.linkCache || /* @__PURE__ */ new Map();
        this.statCache = opt.statCache || /* @__PURE__ */ new Map();
        this.preservePaths = !!opt.preservePaths;
        this.cwd = normalizeWindowsPath(opt.cwd || process.cwd());
        this.strict = !!opt.strict;
        this.noPax = !!opt.noPax;
        this.noMtime = !!opt.noMtime;
        this.mtime = opt.mtime;
        this.prefix = opt.prefix ? normalizeWindowsPath(opt.prefix) : void 0;
        this.onWriteEntry = opt.onWriteEntry;
        if (typeof opt.onwarn === "function") {
          this.on("warn", opt.onwarn);
        }
        let pathWarn = false;
        if (!this.preservePaths) {
          const [root, stripped] = stripAbsolutePath(this.path);
          if (root && typeof stripped === "string") {
            this.path = stripped;
            pathWarn = root;
          }
        }
        this.win32 = !!opt.win32 || process.platform === "win32";
        if (this.win32) {
          this.path = decode(this.path.replaceAll(/\\/g, "/"));
          p = p.replaceAll(/\\/g, "/");
        }
        this.absolute = normalizeWindowsPath(opt.absolute || import_path9.default.resolve(this.cwd, p));
        if (this.path === "") {
          this.path = "./";
        }
        if (pathWarn) {
          this.warn("TAR_ENTRY_INFO", `stripping ${pathWarn} from absolute path`, {
            entry: this,
            path: pathWarn + this.path
          });
        }
        const cs = this.statCache.get(this.absolute);
        if (cs) {
          this[ONLSTAT](cs);
        } else {
          this[LSTAT]();
        }
      }
      warn(code2, message, data = {}) {
        return warnMethod(this, code2, message, data);
      }
      emit(ev, ...data) {
        if (ev === "error") {
          this.#hadError = true;
        }
        return super.emit(ev, ...data);
      }
      [LSTAT]() {
        import_fs11.default.lstat(this.absolute, (er, stat) => {
          if (er) {
            return this.emit("error", er);
          }
          this[ONLSTAT](stat);
        });
      }
      [ONLSTAT](stat) {
        this.statCache.set(this.absolute, stat);
        this.stat = stat;
        if (!stat.isFile()) {
          stat.size = 0;
        }
        this.type = getType(stat);
        this.emit("stat", stat);
        this[PROCESS]();
      }
      [PROCESS]() {
        switch (this.type) {
          case "File":
            return this[FILE2]();
          case "Directory":
            return this[DIRECTORY2]();
          case "SymbolicLink":
            return this[SYMLINK2]();
          // unsupported types are ignored.
          default:
            return this.end();
        }
      }
      [MODE](mode) {
        return modeFix(mode, this.type === "Directory", this.portable);
      }
      [PREFIX](path16) {
        return prefixPath(path16, this.prefix);
      }
      [HEADER]() {
        if (!this.stat) {
          throw new Error("cannot write header before stat");
        }
        if (this.type === "Directory" && this.portable) {
          this.noMtime = true;
        }
        this.onWriteEntry?.(this);
        this.header = new Header({
          path: this[PREFIX](this.path),
          // only apply the prefix to hard links.
          linkpath: this.type === "Link" && this.linkpath !== void 0 ? this[PREFIX](this.linkpath) : this.linkpath,
          // only the permissions and setuid/setgid/sticky bitflags
          // not the higher-order bits that specify file type
          mode: this[MODE](this.stat.mode),
          uid: this.portable ? void 0 : this.stat.uid,
          gid: this.portable ? void 0 : this.stat.gid,
          size: this.stat.size,
          mtime: this.noMtime ? void 0 : this.mtime || this.stat.mtime,
          /* c8 ignore next */
          type: this.type === "Unsupported" ? void 0 : this.type,
          uname: this.portable ? void 0 : this.stat.uid === this.myuid ? this.myuser : "",
          atime: this.portable ? void 0 : this.stat.atime,
          ctime: this.portable ? void 0 : this.stat.ctime
        });
        if (this.header.encode() && !this.noPax) {
          super.write(new Pax({
            atime: this.portable ? void 0 : this.header.atime,
            ctime: this.portable ? void 0 : this.header.ctime,
            gid: this.portable ? void 0 : this.header.gid,
            mtime: this.noMtime ? void 0 : this.mtime || this.header.mtime,
            path: this[PREFIX](this.path),
            linkpath: this.type === "Link" && this.linkpath !== void 0 ? this[PREFIX](this.linkpath) : this.linkpath,
            size: this.header.size,
            uid: this.portable ? void 0 : this.header.uid,
            uname: this.portable ? void 0 : this.header.uname,
            dev: this.portable ? void 0 : this.stat.dev,
            ino: this.portable ? void 0 : this.stat.ino,
            nlink: this.portable ? void 0 : this.stat.nlink
          }).encode());
        }
        const block = this.header?.block;
        if (!block) {
          throw new Error("failed to encode header");
        }
        super.write(block);
      }
      [DIRECTORY2]() {
        if (!this.stat) {
          throw new Error("cannot create directory entry without stat");
        }
        if (this.path.slice(-1) !== "/") {
          this.path += "/";
        }
        this.stat.size = 0;
        this[HEADER]();
        this.end();
      }
      [SYMLINK2]() {
        import_fs11.default.readlink(this.absolute, (er, linkpath) => {
          if (er) {
            return this.emit("error", er);
          }
          this[ONREADLINK](linkpath);
        });
      }
      [ONREADLINK](linkpath) {
        this.linkpath = normalizeWindowsPath(linkpath);
        this[HEADER]();
        this.end();
      }
      [HARDLINK2](linkpath) {
        if (!this.stat) {
          throw new Error("cannot create link entry without stat");
        }
        this.type = "Link";
        this.linkpath = normalizeWindowsPath(import_path9.default.relative(this.cwd, linkpath));
        this.stat.size = 0;
        this[HEADER]();
        this.end();
      }
      [FILE2]() {
        if (!this.stat) {
          throw new Error("cannot create file entry without stat");
        }
        if (this.stat.nlink > 1) {
          const linkKey = `${this.stat.dev}:${this.stat.ino}`;
          const linkpath = this.linkCache.get(linkKey);
          if (linkpath?.indexOf(this.cwd) === 0) {
            return this[HARDLINK2](linkpath);
          }
          this.linkCache.set(linkKey, this.absolute);
        }
        this[HEADER]();
        if (this.stat.size === 0) {
          return this.end();
        }
        this[OPENFILE]();
      }
      [OPENFILE]() {
        import_fs11.default.open(this.absolute, "r", (er, fd) => {
          if (er) {
            return this.emit("error", er);
          }
          this[ONOPENFILE](fd);
        });
      }
      [ONOPENFILE](fd) {
        this.fd = fd;
        if (this.#hadError) {
          return this[CLOSE]();
        }
        if (!this.stat) {
          throw new Error("should stat before calling onopenfile");
        }
        this.blockLen = 512 * Math.ceil(this.stat.size / 512);
        this.blockRemain = this.blockLen;
        const bufLen = Math.min(this.blockLen, this.maxReadSize);
        this.buf = Buffer.allocUnsafe(bufLen);
        this.offset = 0;
        this.pos = 0;
        this.remain = this.stat.size;
        this.length = this.buf.length;
        this[READ2]();
      }
      [READ2]() {
        const { fd, buf, offset, length, pos: pos2 } = this;
        if (fd === void 0 || buf === void 0) {
          throw new Error("cannot read file without first opening");
        }
        import_fs11.default.read(fd, buf, offset, length, pos2, (er, bytesRead) => {
          if (er) {
            return this[CLOSE](() => this.emit("error", er));
          }
          this[ONREAD](bytesRead);
        });
      }
      /* c8 ignore start */
      [CLOSE](cb = () => {
      }) {
        if (this.fd !== void 0)
          import_fs11.default.close(this.fd, cb);
      }
      [ONREAD](bytesRead) {
        if (bytesRead <= 0 && this.remain > 0) {
          const er = Object.assign(new Error("encountered unexpected EOF"), {
            path: this.absolute,
            syscall: "read",
            code: "EOF"
          });
          return this[CLOSE](() => this.emit("error", er));
        }
        if (bytesRead > this.remain) {
          const er = Object.assign(new Error("did not encounter expected EOF"), {
            path: this.absolute,
            syscall: "read",
            code: "EOF"
          });
          return this[CLOSE](() => this.emit("error", er));
        }
        if (!this.buf) {
          throw new Error("should have created buffer prior to reading");
        }
        if (bytesRead === this.remain) {
          for (let i = bytesRead; i < this.length && bytesRead < this.blockRemain; i++) {
            this.buf[i + this.offset] = 0;
            bytesRead++;
            this.remain++;
          }
        }
        const chunk = this.offset === 0 && bytesRead === this.buf.length ? this.buf : this.buf.subarray(this.offset, this.offset + bytesRead);
        const flushed = this.write(chunk);
        if (!flushed) {
          this[AWAITDRAIN](() => this[ONDRAIN]());
        } else {
          this[ONDRAIN]();
        }
      }
      [AWAITDRAIN](cb) {
        this.once("drain", cb);
      }
      write(chunk, encoding, cb) {
        if (typeof encoding === "function") {
          cb = encoding;
          encoding = void 0;
        }
        if (typeof chunk === "string") {
          chunk = Buffer.from(chunk, typeof encoding === "string" ? encoding : "utf8");
        }
        if (this.blockRemain < chunk.length) {
          const er = Object.assign(new Error("writing more data than expected"), {
            path: this.absolute
          });
          return this.emit("error", er);
        }
        this.remain -= chunk.length;
        this.blockRemain -= chunk.length;
        this.pos += chunk.length;
        this.offset += chunk.length;
        return super.write(chunk, null, cb);
      }
      [ONDRAIN]() {
        if (!this.remain) {
          if (this.blockRemain) {
            super.write(Buffer.alloc(this.blockRemain));
          }
          return this[CLOSE]((er) => er ? this.emit("error", er) : this.end());
        }
        if (!this.buf) {
          throw new Error("buffer lost somehow in ONDRAIN");
        }
        if (this.offset >= this.length) {
          this.buf = Buffer.allocUnsafe(Math.min(this.blockRemain, this.buf.length));
          this.offset = 0;
        }
        this.length = this.buf.length - this.offset;
        this[READ2]();
      }
    };
    WriteEntrySync = class extends WriteEntry {
      sync = true;
      [LSTAT]() {
        this[ONLSTAT](import_fs11.default.lstatSync(this.absolute));
      }
      [SYMLINK2]() {
        this[ONREADLINK](import_fs11.default.readlinkSync(this.absolute));
      }
      [OPENFILE]() {
        this[ONOPENFILE](import_fs11.default.openSync(this.absolute, "r"));
      }
      [READ2]() {
        let threw = true;
        try {
          const { fd, buf, offset, length, pos: pos2 } = this;
          if (fd === void 0 || buf === void 0) {
            throw new Error("fd and buf must be set in READ method");
          }
          const bytesRead = import_fs11.default.readSync(fd, buf, offset, length, pos2);
          this[ONREAD](bytesRead);
          threw = false;
        } finally {
          if (threw) {
            try {
              this[CLOSE](() => {
              });
            } catch {
            }
          }
        }
      }
      [AWAITDRAIN](cb) {
        cb();
      }
      /* c8 ignore start */
      [CLOSE](cb = () => {
      }) {
        if (this.fd !== void 0)
          import_fs11.default.closeSync(this.fd);
        cb();
      }
    };
    WriteEntryTar = class extends Minipass {
      blockLen = 0;
      blockRemain = 0;
      buf = 0;
      pos = 0;
      remain = 0;
      length = 0;
      preservePaths;
      portable;
      strict;
      noPax;
      noMtime;
      readEntry;
      type;
      prefix;
      path;
      mode;
      uid;
      gid;
      uname;
      gname;
      header;
      mtime;
      atime;
      ctime;
      linkpath;
      size;
      onWriteEntry;
      warn(code2, message, data = {}) {
        return warnMethod(this, code2, message, data);
      }
      constructor(readEntry, opt_ = {}) {
        const opt = dealias(opt_);
        super();
        this.preservePaths = !!opt.preservePaths;
        this.portable = !!opt.portable;
        this.strict = !!opt.strict;
        this.noPax = !!opt.noPax;
        this.noMtime = !!opt.noMtime;
        this.onWriteEntry = opt.onWriteEntry;
        this.readEntry = readEntry;
        const { type } = readEntry;
        if (type === "Unsupported") {
          throw new Error("writing entry that should be ignored");
        }
        this.type = type;
        if (this.type === "Directory" && this.portable) {
          this.noMtime = true;
        }
        this.prefix = opt.prefix;
        this.path = normalizeWindowsPath(readEntry.path);
        this.mode = readEntry.mode !== void 0 ? this[MODE](readEntry.mode) : void 0;
        this.uid = this.portable ? void 0 : readEntry.uid;
        this.gid = this.portable ? void 0 : readEntry.gid;
        this.uname = this.portable ? void 0 : readEntry.uname;
        this.gname = this.portable ? void 0 : readEntry.gname;
        this.size = readEntry.size;
        this.mtime = this.noMtime ? void 0 : opt.mtime || readEntry.mtime;
        this.atime = this.portable ? void 0 : readEntry.atime;
        this.ctime = this.portable ? void 0 : readEntry.ctime;
        this.linkpath = readEntry.linkpath !== void 0 ? normalizeWindowsPath(readEntry.linkpath) : void 0;
        if (typeof opt.onwarn === "function") {
          this.on("warn", opt.onwarn);
        }
        let pathWarn = false;
        if (!this.preservePaths) {
          const [root, stripped] = stripAbsolutePath(this.path);
          if (root && typeof stripped === "string") {
            this.path = stripped;
            pathWarn = root;
          }
        }
        this.remain = readEntry.size;
        this.blockRemain = readEntry.startBlockSize;
        this.onWriteEntry?.(this);
        this.header = new Header({
          path: this[PREFIX](this.path),
          linkpath: this.type === "Link" && this.linkpath !== void 0 ? this[PREFIX](this.linkpath) : this.linkpath,
          // only the permissions and setuid/setgid/sticky bitflags
          // not the higher-order bits that specify file type
          mode: this.mode,
          uid: this.portable ? void 0 : this.uid,
          gid: this.portable ? void 0 : this.gid,
          size: this.size,
          mtime: this.noMtime ? void 0 : this.mtime,
          type: this.type,
          uname: this.portable ? void 0 : this.uname,
          atime: this.portable ? void 0 : this.atime,
          ctime: this.portable ? void 0 : this.ctime
        });
        if (pathWarn) {
          this.warn("TAR_ENTRY_INFO", `stripping ${pathWarn} from absolute path`, {
            entry: this,
            path: pathWarn + this.path
          });
        }
        if (this.header.encode() && !this.noPax) {
          super.write(new Pax({
            atime: this.portable ? void 0 : this.atime,
            ctime: this.portable ? void 0 : this.ctime,
            gid: this.portable ? void 0 : this.gid,
            mtime: this.noMtime ? void 0 : this.mtime,
            path: this[PREFIX](this.path),
            linkpath: this.type === "Link" && this.linkpath !== void 0 ? this[PREFIX](this.linkpath) : this.linkpath,
            size: this.size,
            uid: this.portable ? void 0 : this.uid,
            uname: this.portable ? void 0 : this.uname,
            dev: this.portable ? void 0 : this.readEntry.dev,
            ino: this.portable ? void 0 : this.readEntry.ino,
            nlink: this.portable ? void 0 : this.readEntry.nlink
          }).encode());
        }
        const b = this.header?.block;
        if (!b)
          throw new Error("failed to encode header");
        super.write(b);
        readEntry.pipe(this);
      }
      [PREFIX](path16) {
        return prefixPath(path16, this.prefix);
      }
      [MODE](mode) {
        return modeFix(mode, this.type === "Directory", this.portable);
      }
      write(chunk, encoding, cb) {
        if (typeof encoding === "function") {
          cb = encoding;
          encoding = void 0;
        }
        if (typeof chunk === "string") {
          chunk = Buffer.from(chunk, typeof encoding === "string" ? encoding : "utf8");
        }
        const writeLen = chunk.length;
        if (writeLen > this.blockRemain) {
          throw new Error("writing more to entry than is appropriate");
        }
        this.blockRemain -= writeLen;
        return super.write(chunk, cb);
      }
      end(chunk, encoding, cb) {
        if (this.blockRemain) {
          super.write(Buffer.alloc(this.blockRemain));
        }
        if (typeof chunk === "function") {
          cb = chunk;
          encoding = void 0;
          chunk = void 0;
        }
        if (typeof encoding === "function") {
          cb = encoding;
          encoding = void 0;
        }
        if (typeof chunk === "string") {
          chunk = Buffer.from(chunk, encoding ?? "utf8");
        }
        if (cb)
          this.once("finish", cb);
        if (chunk)
          super.end(chunk, cb);
        else
          super.end(cb);
        return this;
      }
    };
    getType = (stat) => stat.isFile() ? "File" : stat.isDirectory() ? "Directory" : stat.isSymbolicLink() ? "SymbolicLink" : "Unsupported";
  }
});

// node_modules/yallist/dist/esm/index.js
function insertAfter(self2, node, value) {
  const prev = node;
  const next = node ? node.next : self2.head;
  const inserted = new Node(value, prev, next, self2);
  if (inserted.next === void 0) {
    self2.tail = inserted;
  }
  if (inserted.prev === void 0) {
    self2.head = inserted;
  }
  self2.length++;
  return inserted;
}
function push(self2, item) {
  self2.tail = new Node(item, self2.tail, void 0, self2);
  if (!self2.head) {
    self2.head = self2.tail;
  }
  self2.length++;
}
function unshift(self2, item) {
  self2.head = new Node(item, void 0, self2.head, self2);
  if (!self2.tail) {
    self2.tail = self2.head;
  }
  self2.length++;
}
var Yallist, Node;
var init_esm5 = __esm({
  "node_modules/yallist/dist/esm/index.js"() {
    Yallist = class _Yallist {
      tail;
      head;
      length = 0;
      static create(list2 = []) {
        return new _Yallist(list2);
      }
      constructor(list2 = []) {
        for (const item of list2) {
          this.push(item);
        }
      }
      *[Symbol.iterator]() {
        for (let walker = this.head; walker; walker = walker.next) {
          yield walker.value;
        }
      }
      removeNode(node) {
        if (node.list !== this) {
          throw new Error("removing node which does not belong to this list");
        }
        const next = node.next;
        const prev = node.prev;
        if (next) {
          next.prev = prev;
        }
        if (prev) {
          prev.next = next;
        }
        if (node === this.head) {
          this.head = next;
        }
        if (node === this.tail) {
          this.tail = prev;
        }
        this.length--;
        node.next = void 0;
        node.prev = void 0;
        node.list = void 0;
        return next;
      }
      unshiftNode(node) {
        if (node === this.head) {
          return;
        }
        if (node.list) {
          node.list.removeNode(node);
        }
        const head = this.head;
        node.list = this;
        node.next = head;
        if (head) {
          head.prev = node;
        }
        this.head = node;
        if (!this.tail) {
          this.tail = node;
        }
        this.length++;
      }
      pushNode(node) {
        if (node === this.tail) {
          return;
        }
        if (node.list) {
          node.list.removeNode(node);
        }
        const tail = this.tail;
        node.list = this;
        node.prev = tail;
        if (tail) {
          tail.next = node;
        }
        this.tail = node;
        if (!this.head) {
          this.head = node;
        }
        this.length++;
      }
      push(...args) {
        for (let i = 0, l = args.length; i < l; i++) {
          push(this, args[i]);
        }
        return this.length;
      }
      unshift(...args) {
        for (var i = 0, l = args.length; i < l; i++) {
          unshift(this, args[i]);
        }
        return this.length;
      }
      pop() {
        if (!this.tail) {
          return void 0;
        }
        const res = this.tail.value;
        const t = this.tail;
        this.tail = this.tail.prev;
        if (this.tail) {
          this.tail.next = void 0;
        } else {
          this.head = void 0;
        }
        t.list = void 0;
        this.length--;
        return res;
      }
      shift() {
        if (!this.head) {
          return void 0;
        }
        const res = this.head.value;
        const h = this.head;
        this.head = this.head.next;
        if (this.head) {
          this.head.prev = void 0;
        } else {
          this.tail = void 0;
        }
        h.list = void 0;
        this.length--;
        return res;
      }
      forEach(fn2, thisp) {
        thisp = thisp || this;
        for (let walker = this.head, i = 0; !!walker; i++) {
          fn2.call(thisp, walker.value, i, this);
          walker = walker.next;
        }
      }
      forEachReverse(fn2, thisp) {
        thisp = thisp || this;
        for (let walker = this.tail, i = this.length - 1; !!walker; i--) {
          fn2.call(thisp, walker.value, i, this);
          walker = walker.prev;
        }
      }
      get(n) {
        let i = 0;
        let walker = this.head;
        for (; !!walker && i < n; i++) {
          walker = walker.next;
        }
        if (i === n && !!walker) {
          return walker.value;
        }
      }
      getReverse(n) {
        let i = 0;
        let walker = this.tail;
        for (; !!walker && i < n; i++) {
          walker = walker.prev;
        }
        if (i === n && !!walker) {
          return walker.value;
        }
      }
      map(fn2, thisp) {
        thisp = thisp || this;
        const res = new _Yallist();
        for (let walker = this.head; !!walker; ) {
          res.push(fn2.call(thisp, walker.value, this));
          walker = walker.next;
        }
        return res;
      }
      mapReverse(fn2, thisp) {
        thisp = thisp || this;
        var res = new _Yallist();
        for (let walker = this.tail; !!walker; ) {
          res.push(fn2.call(thisp, walker.value, this));
          walker = walker.prev;
        }
        return res;
      }
      reduce(fn2, initial) {
        let acc;
        let walker = this.head;
        if (arguments.length > 1) {
          acc = initial;
        } else if (this.head) {
          walker = this.head.next;
          acc = this.head.value;
        } else {
          throw new TypeError("Reduce of empty list with no initial value");
        }
        for (var i = 0; !!walker; i++) {
          acc = fn2(acc, walker.value, i);
          walker = walker.next;
        }
        return acc;
      }
      reduceReverse(fn2, initial) {
        let acc;
        let walker = this.tail;
        if (arguments.length > 1) {
          acc = initial;
        } else if (this.tail) {
          walker = this.tail.prev;
          acc = this.tail.value;
        } else {
          throw new TypeError("Reduce of empty list with no initial value");
        }
        for (let i = this.length - 1; !!walker; i--) {
          acc = fn2(acc, walker.value, i);
          walker = walker.prev;
        }
        return acc;
      }
      toArray() {
        const arr = new Array(this.length);
        for (let i = 0, walker = this.head; !!walker; i++) {
          arr[i] = walker.value;
          walker = walker.next;
        }
        return arr;
      }
      toArrayReverse() {
        const arr = new Array(this.length);
        for (let i = 0, walker = this.tail; !!walker; i++) {
          arr[i] = walker.value;
          walker = walker.prev;
        }
        return arr;
      }
      slice(from = 0, to = this.length) {
        if (to < 0) {
          to += this.length;
        }
        if (from < 0) {
          from += this.length;
        }
        const ret = new _Yallist();
        if (to < from || to < 0) {
          return ret;
        }
        if (from < 0) {
          from = 0;
        }
        if (to > this.length) {
          to = this.length;
        }
        let walker = this.head;
        let i = 0;
        for (i = 0; !!walker && i < from; i++) {
          walker = walker.next;
        }
        for (; !!walker && i < to; i++, walker = walker.next) {
          ret.push(walker.value);
        }
        return ret;
      }
      sliceReverse(from = 0, to = this.length) {
        if (to < 0) {
          to += this.length;
        }
        if (from < 0) {
          from += this.length;
        }
        const ret = new _Yallist();
        if (to < from || to < 0) {
          return ret;
        }
        if (from < 0) {
          from = 0;
        }
        if (to > this.length) {
          to = this.length;
        }
        let i = this.length;
        let walker = this.tail;
        for (; !!walker && i > to; i--) {
          walker = walker.prev;
        }
        for (; !!walker && i > from; i--, walker = walker.prev) {
          ret.push(walker.value);
        }
        return ret;
      }
      splice(start, deleteCount = 0, ...nodes) {
        if (start > this.length) {
          start = this.length - 1;
        }
        if (start < 0) {
          start = this.length + start;
        }
        let walker = this.head;
        for (let i = 0; !!walker && i < start; i++) {
          walker = walker.next;
        }
        const ret = [];
        for (let i = 0; !!walker && i < deleteCount; i++) {
          ret.push(walker.value);
          walker = this.removeNode(walker);
        }
        if (!walker) {
          walker = this.tail;
        } else if (walker !== this.tail) {
          walker = walker.prev;
        }
        for (const v of nodes) {
          walker = insertAfter(this, walker, v);
        }
        return ret;
      }
      reverse() {
        const head = this.head;
        const tail = this.tail;
        for (let walker = head; !!walker; walker = walker.prev) {
          const p = walker.prev;
          walker.prev = walker.next;
          walker.next = p;
        }
        this.head = tail;
        this.tail = head;
        return this;
      }
    };
    Node = class {
      list;
      next;
      prev;
      value;
      constructor(value, prev, next, list2) {
        this.list = list2;
        this.value = value;
        if (prev) {
          prev.next = this;
          this.prev = prev;
        } else {
          this.prev = void 0;
        }
        if (next) {
          next.prev = this;
          this.next = next;
        } else {
          this.next = void 0;
        }
      }
    };
  }
});

// node_modules/tar/dist/esm/pack.js
var import_fs12, import_path10, PackJob, EOF2, ONSTAT, ENDED3, QUEUE2, PENDINGLINKS, CURRENT, PROCESS2, PROCESSING, PROCESSJOB, JOBS, JOBDONE, ADDFSENTRY, ADDTARENTRY, STAT, READDIR, ONREADDIR, PIPE, ENTRY, ENTRYOPT, WRITEENTRYCLASS, WRITE, ONDRAIN2, Pack, PackSync;
var init_pack = __esm({
  "node_modules/tar/dist/esm/pack.js"() {
    import_fs12 = __toESM(require("fs"), 1);
    init_write_entry();
    init_esm();
    init_esm3();
    init_esm5();
    init_read_entry();
    init_warn_method();
    import_path10 = __toESM(require("path"), 1);
    init_normalize_windows_path();
    PackJob = class {
      path;
      absolute;
      entry;
      stat;
      readdir;
      pending = false;
      pendingLink = false;
      ignore = false;
      piped = false;
      constructor(path16, absolute) {
        this.path = path16 || "./";
        this.absolute = absolute;
      }
    };
    EOF2 = Buffer.alloc(1024);
    ONSTAT = /* @__PURE__ */ Symbol("onStat");
    ENDED3 = /* @__PURE__ */ Symbol("ended");
    QUEUE2 = /* @__PURE__ */ Symbol("queue");
    PENDINGLINKS = /* @__PURE__ */ Symbol("queue");
    CURRENT = /* @__PURE__ */ Symbol("current");
    PROCESS2 = /* @__PURE__ */ Symbol("process");
    PROCESSING = /* @__PURE__ */ Symbol("processing");
    PROCESSJOB = /* @__PURE__ */ Symbol("processJob");
    JOBS = /* @__PURE__ */ Symbol("jobs");
    JOBDONE = /* @__PURE__ */ Symbol("jobDone");
    ADDFSENTRY = /* @__PURE__ */ Symbol("addFSEntry");
    ADDTARENTRY = /* @__PURE__ */ Symbol("addTarEntry");
    STAT = /* @__PURE__ */ Symbol("stat");
    READDIR = /* @__PURE__ */ Symbol("readdir");
    ONREADDIR = /* @__PURE__ */ Symbol("onreaddir");
    PIPE = /* @__PURE__ */ Symbol("pipe");
    ENTRY = /* @__PURE__ */ Symbol("entry");
    ENTRYOPT = /* @__PURE__ */ Symbol("entryOpt");
    WRITEENTRYCLASS = /* @__PURE__ */ Symbol("writeEntryClass");
    WRITE = /* @__PURE__ */ Symbol("write");
    ONDRAIN2 = /* @__PURE__ */ Symbol("ondrain");
    Pack = class extends Minipass {
      sync = false;
      opt;
      cwd;
      maxReadSize;
      preservePaths;
      strict;
      noPax;
      prefix;
      linkCache;
      statCache;
      file;
      portable;
      zip;
      readdirCache;
      noDirRecurse;
      follow;
      noMtime;
      mtime;
      filter;
      jobs;
      [WRITEENTRYCLASS];
      onWriteEntry;
      // Note: we actually DO need a linked list here, because we
      // shift() to update the head of the list where we start, but still
      // while that happens, need to know what the next item in the queue
      // will be. Since we do multiple jobs in parallel, it's not as simple
      // as just an Array.shift(), since that would lose the information about
      // the next job in the list. We could add a .next field on the PackJob
      // class, but then we'd have to be tracking the tail of the queue the
      // whole time, and Yallist just does that for us anyway.
      [QUEUE2];
      [PENDINGLINKS] = /* @__PURE__ */ new Map();
      [JOBS] = 0;
      [PROCESSING] = false;
      [ENDED3] = false;
      constructor(opt = {}) {
        super();
        this.opt = opt;
        this.file = opt.file || "";
        this.cwd = opt.cwd || process.cwd();
        this.maxReadSize = opt.maxReadSize;
        this.preservePaths = !!opt.preservePaths;
        this.strict = !!opt.strict;
        this.noPax = !!opt.noPax;
        this.prefix = normalizeWindowsPath(opt.prefix || "");
        this.linkCache = opt.linkCache || /* @__PURE__ */ new Map();
        this.statCache = opt.statCache || /* @__PURE__ */ new Map();
        this.readdirCache = opt.readdirCache || /* @__PURE__ */ new Map();
        this.onWriteEntry = opt.onWriteEntry;
        this[WRITEENTRYCLASS] = WriteEntry;
        if (typeof opt.onwarn === "function") {
          this.on("warn", opt.onwarn);
        }
        this.portable = !!opt.portable;
        if (opt.gzip || opt.brotli || opt.zstd) {
          if ((opt.gzip ? 1 : 0) + (opt.brotli ? 1 : 0) + (opt.zstd ? 1 : 0) > 1) {
            throw new TypeError("gzip, brotli, zstd are mutually exclusive");
          }
          if (opt.gzip) {
            if (typeof opt.gzip !== "object") {
              opt.gzip = {};
            }
            if (this.portable) {
              opt.gzip.portable = true;
            }
            this.zip = new Gzip(opt.gzip);
          }
          if (opt.brotli) {
            if (typeof opt.brotli !== "object") {
              opt.brotli = {};
            }
            this.zip = new BrotliCompress(opt.brotli);
          }
          if (opt.zstd) {
            if (typeof opt.zstd !== "object") {
              opt.zstd = {};
            }
            this.zip = new ZstdCompress(opt.zstd);
          }
          if (!this.zip)
            throw new Error("impossible");
          const zip = this.zip;
          zip.on("data", (chunk) => super.write(chunk));
          zip.on("end", () => super.end());
          zip.on("drain", () => this[ONDRAIN2]());
          this.on("resume", () => zip.resume());
        } else {
          this.on("drain", this[ONDRAIN2]);
        }
        this.noDirRecurse = !!opt.noDirRecurse;
        this.follow = !!opt.follow;
        this.noMtime = !!opt.noMtime;
        if (opt.mtime)
          this.mtime = opt.mtime;
        this.filter = typeof opt.filter === "function" ? opt.filter : () => true;
        this[QUEUE2] = new Yallist();
        this[JOBS] = 0;
        this.jobs = Number(opt.jobs) || 4;
        this[PROCESSING] = false;
        this[ENDED3] = false;
      }
      [WRITE](chunk) {
        return super.write(chunk);
      }
      add(path16) {
        this.write(path16);
        return this;
      }
      end(path16, encoding, cb) {
        if (typeof path16 === "function") {
          cb = path16;
          path16 = void 0;
        }
        if (typeof encoding === "function") {
          cb = encoding;
          encoding = void 0;
        }
        if (path16) {
          this.add(path16);
        }
        this[ENDED3] = true;
        this[PROCESS2]();
        if (cb)
          cb();
        return this;
      }
      write(path16) {
        if (this[ENDED3]) {
          throw new Error("write after end");
        }
        if (path16 instanceof ReadEntry) {
          this[ADDTARENTRY](path16);
        } else {
          this[ADDFSENTRY](path16);
        }
        return this.flowing;
      }
      [ADDTARENTRY](p) {
        const absolute = normalizeWindowsPath(import_path10.default.resolve(this.cwd, p.path));
        if (!this.filter(p.path, p)) {
          p.resume();
        } else {
          const job = new PackJob(p.path, absolute);
          job.entry = new WriteEntryTar(p, this[ENTRYOPT](job));
          job.entry.on("end", () => this[JOBDONE](job));
          this[JOBS] += 1;
          this[QUEUE2].push(job);
        }
        this[PROCESS2]();
      }
      [ADDFSENTRY](p) {
        const absolute = normalizeWindowsPath(import_path10.default.resolve(this.cwd, p));
        this[QUEUE2].push(new PackJob(p, absolute));
        this[PROCESS2]();
      }
      [STAT](job) {
        job.pending = true;
        this[JOBS] += 1;
        const stat = this.follow ? "stat" : "lstat";
        import_fs12.default[stat](job.absolute, (er, stat2) => {
          job.pending = false;
          this[JOBS] -= 1;
          if (er) {
            this.emit("error", er);
          } else {
            this[ONSTAT](job, stat2);
          }
        });
      }
      [ONSTAT](job, stat) {
        this.statCache.set(job.absolute, stat);
        job.stat = stat;
        if (!this.filter(job.path, stat)) {
          job.ignore = true;
        } else if (stat.isFile() && stat.nlink > 1 && !this.linkCache.get(`${stat.dev}:${stat.ino}`) && !this.sync) {
          if (job === this[CURRENT]) {
            this[PROCESSJOB](job);
          } else {
            const key = `${stat.dev}:${stat.ino}`;
            const pending = this[PENDINGLINKS].get(key);
            if (pending)
              pending.push(job);
            else
              this[PENDINGLINKS].set(key, [job]);
            job.pendingLink = true;
            job.pending = true;
          }
        }
        this[PROCESS2]();
      }
      [READDIR](job) {
        job.pending = true;
        this[JOBS] += 1;
        import_fs12.default.readdir(job.absolute, (er, entries) => {
          job.pending = false;
          this[JOBS] -= 1;
          if (er) {
            return this.emit("error", er);
          }
          this[ONREADDIR](job, entries);
        });
      }
      [ONREADDIR](job, entries) {
        this.readdirCache.set(job.absolute, entries);
        job.readdir = entries;
        this[PROCESS2]();
      }
      [PROCESS2]() {
        if (this[PROCESSING]) {
          return;
        }
        this[PROCESSING] = true;
        for (let w = this[QUEUE2].head; !!w && this[JOBS] < this.jobs; w = w.next) {
          this[PROCESSJOB](w.value);
          if (w.value.ignore) {
            const p = w.next;
            this[QUEUE2].removeNode(w);
            w.next = p;
          }
        }
        this[PROCESSING] = false;
        if (this[ENDED3] && this[QUEUE2].length === 0 && this[JOBS] === 0) {
          if (this.zip) {
            this.zip.end(EOF2);
          } else {
            super.write(EOF2);
            super.end();
          }
        }
      }
      get [CURRENT]() {
        return this[QUEUE2] && this[QUEUE2].head && this[QUEUE2].head.value;
      }
      [JOBDONE](job) {
        this[QUEUE2].shift();
        this[JOBS] -= 1;
        const { stat } = job;
        if (stat && stat.isFile() && stat.nlink > 1) {
          const key = `${stat.dev}:${stat.ino}`;
          const pending = this[PENDINGLINKS].get(key);
          if (pending) {
            this[PENDINGLINKS].delete(key);
            for (const job2 of pending) {
              job2.pending = false;
              this[PROCESSJOB](job2);
            }
          }
        }
        this[PROCESS2]();
      }
      [PROCESSJOB](job) {
        if (job.pending && job.pendingLink && job === this[CURRENT]) {
          job.pending = false;
          job.pendingLink = false;
        }
        if (job.pending) {
          return;
        }
        if (job.entry) {
          if (job === this[CURRENT] && !job.piped) {
            this[PIPE](job);
          }
          return;
        }
        if (!job.stat) {
          const sc = this.statCache.get(job.absolute);
          if (sc) {
            this[ONSTAT](job, sc);
          } else {
            this[STAT](job);
          }
        }
        if (!job.stat) {
          return;
        }
        if (job.ignore) {
          return;
        }
        if (!this.noDirRecurse && job.stat.isDirectory() && !job.readdir) {
          const rc = this.readdirCache.get(job.absolute);
          if (rc) {
            this[ONREADDIR](job, rc);
          } else {
            this[READDIR](job);
          }
          if (!job.readdir) {
            return;
          }
        }
        job.entry = this[ENTRY](job);
        if (!job.entry) {
          job.ignore = true;
          return;
        }
        if (job === this[CURRENT] && !job.piped) {
          this[PIPE](job);
        }
      }
      [ENTRYOPT](job) {
        return {
          onwarn: (code2, msg, data) => this.warn(code2, msg, data),
          noPax: this.noPax,
          cwd: this.cwd,
          absolute: job.absolute,
          preservePaths: this.preservePaths,
          maxReadSize: this.maxReadSize,
          strict: this.strict,
          portable: this.portable,
          linkCache: this.linkCache,
          statCache: this.statCache,
          noMtime: this.noMtime,
          mtime: this.mtime,
          prefix: this.prefix,
          onWriteEntry: this.onWriteEntry
        };
      }
      [ENTRY](job) {
        this[JOBS] += 1;
        try {
          const e = new this[WRITEENTRYCLASS](job.path, this[ENTRYOPT](job));
          return e.on("end", () => this[JOBDONE](job)).on("error", (er) => this.emit("error", er));
        } catch (er) {
          this.emit("error", er);
        }
      }
      [ONDRAIN2]() {
        if (this[CURRENT] && this[CURRENT].entry) {
          this[CURRENT].entry.resume();
        }
      }
      // like .pipe() but using super, because our write() is special
      [PIPE](job) {
        job.piped = true;
        if (job.readdir) {
          job.readdir.forEach((entry) => {
            const p = job.path;
            const base = p === "./" ? "" : p.replace(/\/*$/, "/");
            this[ADDFSENTRY](base + entry);
          });
        }
        const source = job.entry;
        const zip = this.zip;
        if (!source)
          throw new Error("cannot pipe without source");
        if (zip) {
          source.on("data", (chunk) => {
            if (!zip.write(chunk)) {
              source.pause();
            }
          });
        } else {
          source.on("data", (chunk) => {
            if (!super.write(chunk)) {
              source.pause();
            }
          });
        }
      }
      pause() {
        if (this.zip) {
          this.zip.pause();
        }
        return super.pause();
      }
      warn(code2, message, data = {}) {
        warnMethod(this, code2, message, data);
      }
    };
    PackSync = class extends Pack {
      sync = true;
      constructor(opt) {
        super(opt);
        this[WRITEENTRYCLASS] = WriteEntrySync;
      }
      // pause/resume are no-ops in sync streams.
      pause() {
      }
      resume() {
      }
      [STAT](job) {
        const stat = this.follow ? "statSync" : "lstatSync";
        this[ONSTAT](job, import_fs12.default[stat](job.absolute));
      }
      [READDIR](job) {
        this[ONREADDIR](job, import_fs12.default.readdirSync(job.absolute));
      }
      // gotta get it all in this tick
      [PIPE](job) {
        const source = job.entry;
        const zip = this.zip;
        if (job.readdir) {
          job.readdir.forEach((entry) => {
            const p = job.path;
            const base = p === "./" ? "" : p.replace(/\/*$/, "/");
            this[ADDFSENTRY](base + entry);
          });
        }
        if (!source)
          throw new Error("Cannot pipe without source");
        if (zip) {
          source.on("data", (chunk) => {
            zip.write(chunk);
          });
        } else {
          source.on("data", (chunk) => {
            super[WRITE](chunk);
          });
        }
      }
    };
  }
});

// node_modules/tar/dist/esm/create.js
var create_exports = {};
__export(create_exports, {
  create: () => create
});
var import_node_path8, createFileSync, createFile, addFilesSync, addFilesAsync, createSync, createAsync, create;
var init_create = __esm({
  "node_modules/tar/dist/esm/create.js"() {
    init_esm2();
    import_node_path8 = __toESM(require("node:path"), 1);
    init_list();
    init_make_command();
    init_pack();
    createFileSync = (opt, files) => {
      const p = new PackSync(opt);
      const stream = new WriteStreamSync(opt.file, {
        mode: opt.mode || 438
      });
      p.pipe(stream);
      addFilesSync(p, files);
    };
    createFile = (opt, files) => {
      const p = new Pack(opt);
      const stream = new WriteStream(opt.file, {
        mode: opt.mode || 438
      });
      p.pipe(stream);
      const promise = new Promise((res, rej) => {
        stream.on("error", rej);
        stream.on("close", res);
        p.on("error", rej);
      });
      addFilesAsync(p, files).catch((er) => p.emit("error", er));
      return promise;
    };
    addFilesSync = (p, files) => {
      files.forEach((file) => {
        if (file.charAt(0) === "@") {
          list({
            file: import_node_path8.default.resolve(p.cwd, file.slice(1)),
            sync: true,
            noResume: true,
            onReadEntry: (entry) => p.add(entry)
          });
        } else {
          p.add(file);
        }
      });
      p.end();
    };
    addFilesAsync = async (p, files) => {
      for (const file of files) {
        if (file.charAt(0) === "@") {
          await list({
            file: import_node_path8.default.resolve(String(p.cwd), file.slice(1)),
            noResume: true,
            onReadEntry: (entry) => {
              p.add(entry);
            }
          });
        } else {
          p.add(file);
        }
      }
      p.end();
    };
    createSync = (opt, files) => {
      const p = new PackSync(opt);
      addFilesSync(p, files);
      return p;
    };
    createAsync = (opt, files) => {
      const p = new Pack(opt);
      addFilesAsync(p, files).catch((er) => p.emit("error", er));
      return p;
    };
    create = makeCommand(createFileSync, createFile, createSync, createAsync, (_opt, files) => {
      if (!files?.length) {
        throw new TypeError("no paths specified to add to archive");
      }
    });
  }
});

// node_modules/semver/functions/major.js
var require_major = __commonJS({
  "node_modules/semver/functions/major.js"(exports2, module2) {
    "use strict";
    var SemVer3 = require_semver();
    var major = (a, loose) => new SemVer3(a, loose).major;
    module2.exports = major;
  }
});

// sources/_lib.ts
var lib_exports2 = {};
__export(lib_exports2, {
  runMain: () => runMain
});
module.exports = __toCommonJS(lib_exports2);

// sources/main.ts
var import_clipanion17 = __toESM(require_advanced());

// package.json
var version = "0.35.0";

// sources/Engine.ts
var import_clipanion5 = __toESM(require_advanced());
var import_fs6 = __toESM(require("fs"));
var import_path5 = __toESM(require("path"));
var import_process3 = __toESM(require("process"));
var import_rcompare = __toESM(require_rcompare());
var import_valid3 = __toESM(require_valid());
var import_valid4 = __toESM(require_valid2());

// config.json
var config_default = {
  definitions: {
    npm: {
      default: "11.14.1+sha1.4a6839650da0005f323fec6abd39d77ee24f842f",
      fetchLatestFrom: {
        type: "npm",
        package: "npm"
      },
      transparent: {
        commands: [
          [
            "npm",
            "init"
          ],
          [
            "npx"
          ]
        ]
      },
      ranges: {
        "*": {
          url: "https://registry.npmjs.org/npm/-/npm-{}.tgz",
          bin: {
            npm: "./bin/npm-cli.js",
            npx: "./bin/npx-cli.js"
          },
          registry: {
            type: "npm",
            package: "npm"
          },
          commands: {
            use: [
              "npm",
              "install"
            ]
          }
        }
      }
    },
    pnpm: {
      default: "11.1.2+sha1.ed39d701687311ce9345771c62376f9fe7286694",
      fetchLatestFrom: {
        type: "npm",
        package: "pnpm"
      },
      transparent: {
        commands: [
          [
            "pnpm",
            "init"
          ],
          [
            "pnpx"
          ],
          [
            "pnpm",
            "dlx"
          ]
        ]
      },
      ranges: {
        "<6.0.0": {
          url: "https://registry.npmjs.org/pnpm/-/pnpm-{}.tgz",
          bin: {
            pnpm: "./bin/pnpm.js",
            pnpx: "./bin/pnpx.js"
          },
          registry: {
            type: "npm",
            package: "pnpm"
          },
          commands: {
            use: [
              "pnpm",
              "install"
            ]
          }
        },
        "6.x || 7.x || 8.x || 9.x || 10.x": {
          url: "https://registry.npmjs.org/pnpm/-/pnpm-{}.tgz",
          bin: {
            pnpm: "./bin/pnpm.cjs",
            pnpx: "./bin/pnpx.cjs"
          },
          registry: {
            type: "npm",
            package: "pnpm"
          },
          commands: {
            use: [
              "pnpm",
              "install"
            ]
          }
        },
        ">=11.0.0": {
          url: "https://registry.npmjs.org/pnpm/-/pnpm-{}.tgz",
          bin: {
            pnpm: "./bin/pnpm.mjs",
            pnpx: "./bin/pnpx.mjs"
          },
          registry: {
            type: "npm",
            package: "pnpm"
          },
          commands: {
            use: [
              "pnpm",
              "install"
            ]
          }
        }
      }
    },
    yarn: {
      default: "1.22.22+sha1.ac34549e6aa8e7ead463a7407e1c7390f61a6610",
      fetchLatestFrom: {
        type: "npm",
        package: "yarn"
      },
      transparent: {
        default: "4.14.1+sha224.88b7a7244bbd9040380c417f7eb556d85c67640b651f113cb4c72113",
        commands: [
          [
            "yarn",
            "init"
          ],
          [
            "yarn",
            "dlx"
          ]
        ]
      },
      ranges: {
        "<2.0.0": {
          url: "https://registry.yarnpkg.com/yarn/-/yarn-{}.tgz",
          bin: {
            yarn: "./bin/yarn.js",
            yarnpkg: "./bin/yarn.js"
          },
          registry: {
            type: "npm",
            package: "yarn"
          },
          commands: {
            use: [
              "yarn",
              "install"
            ]
          }
        },
        ">=2.0.0": {
          name: "yarn",
          url: "https://repo.yarnpkg.com/{}/packages/yarnpkg-cli/bin/yarn.js",
          bin: [
            "yarn",
            "yarnpkg"
          ],
          registry: {
            type: "url",
            url: "https://repo.yarnpkg.com/tags",
            fields: {
              tags: "aliases",
              versions: "tags"
            }
          },
          npmRegistry: {
            type: "npm",
            package: "@yarnpkg/cli-dist",
            bin: "bin/yarn.js"
          },
          commands: {
            use: [
              "yarn",
              "install"
            ]
          }
        }
      }
    }
  },
  keys: {
    npm: [
      {
        expires: "2025-01-29T00:00:00.000Z",
        keyid: "SHA256:jl3bwswu80PjjokCgh0o2w5c2U4LhQAE57gj9cz1kzA",
        keytype: "ecdsa-sha2-nistp256",
        scheme: "ecdsa-sha2-nistp256",
        key: "MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAE1Olb3zMAFFxXKHiIkQO5cJ3Yhl5i6UPp+IhuteBJbuHcA5UogKo0EWtlWwW6KSaKoTNEYL7JlCQiVnkhBktUgg=="
      },
      {
        expires: null,
        keyid: "SHA256:DhQ8wR5APBvFHLF/+Tc+AYvPOdTpcIDqOhxsBHRwC7U",
        keytype: "ecdsa-sha2-nistp256",
        scheme: "ecdsa-sha2-nistp256",
        key: "MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEY6Ya7W++7aUPzvMTrezH6Ycx3c+HOKYCcNGybJZSCJq/fd7Qa8uuAKtdIkUQtQiEKERhAmE5lMMJhP8OkDOa2g=="
      }
    ]
  }
};

// sources/corepackUtils.ts
var import_crypto2 = require("crypto");
var import_events4 = require("events");
var import_fs4 = __toESM(require("fs"));
var import_module = __toESM(require("module"));
var import_path3 = __toESM(require("path"));
var import_range = __toESM(require_range());
var import_semver = __toESM(require_semver());
var import_lt = __toESM(require_lt());
var import_parse3 = __toESM(require_parse());
var import_promises2 = require("timers/promises");

// sources/debugUtils.ts
var import_debug = __toESM(require_src());
var log = (0, import_debug.default)(`corepack`);

// sources/folderUtils.ts
var import_clipanion = __toESM(require_advanced());
var import_fs = require("fs");
var import_os = require("os");
var import_path = require("path");
var import_process = __toESM(require("process"));
var INSTALL_FOLDER_VERSION = 1;
function getCorepackHomeFolder() {
  return import_process.default.env.COREPACK_HOME ?? (0, import_path.join)(
    import_process.default.env.XDG_CACHE_HOME ?? import_process.default.env.LOCALAPPDATA ?? (0, import_path.join)((0, import_os.homedir)(), import_process.default.platform === `win32` ? `AppData/Local` : `.cache`),
    `node/corepack`
  );
}
function getInstallFolder() {
  return (0, import_path.join)(
    getCorepackHomeFolder(),
    `v${INSTALL_FOLDER_VERSION}`
  );
}
function getTemporaryFolder(target = (0, import_os.tmpdir)()) {
  (0, import_fs.mkdirSync)(target, { recursive: true });
  while (true) {
    const rnd = Math.random() * 4294967296;
    const hex = rnd.toString(16).padStart(8, `0`);
    const path16 = (0, import_path.join)(target, `corepack-${import_process.default.pid}-${hex}`);
    try {
      (0, import_fs.mkdirSync)(path16);
      return path16;
    } catch (error) {
      if (error.code === `EEXIST`) {
        continue;
      } else if (error.code === `EACCES`) {
        throw new import_clipanion.UsageError(`Failed to create cache directory. Please ensure the user has write access to the target directory (${target}). If the user's home directory does not exist, create it first.`);
      } else {
        throw error;
      }
    }
  }
}

// sources/httpUtils.ts
var import_assert = __toESM(require("assert"));
var import_clipanion3 = __toESM(require_advanced());
var import_events = require("events");
var import_process2 = require("process");
var import_stream = require("stream");

// sources/npmRegistryUtils.ts
var import_clipanion2 = __toESM(require_advanced());
var import_crypto = require("crypto");
var DEFAULT_HEADERS = {
  [`Accept`]: `application/vnd.npm.install-v1+json; q=1.0, application/json; q=0.8`
};
var DEFAULT_NPM_REGISTRY_URL = `https://registry.npmjs.org`;
async function fetchAsJson2(packageName, version2) {
  const npmRegistryUrl = process.env.COREPACK_NPM_REGISTRY || DEFAULT_NPM_REGISTRY_URL;
  if (process.env.COREPACK_ENABLE_NETWORK === `0`)
    throw new import_clipanion2.UsageError(`Network access disabled by the environment; can't reach npm repository ${npmRegistryUrl}`);
  const headers = { ...DEFAULT_HEADERS };
  if (`COREPACK_NPM_TOKEN` in process.env) {
    headers.authorization = `Bearer ${process.env.COREPACK_NPM_TOKEN}`;
  } else if (`COREPACK_NPM_USERNAME` in process.env && `COREPACK_NPM_PASSWORD` in process.env) {
    const encodedCreds = Buffer.from(`${process.env.COREPACK_NPM_USERNAME}:${process.env.COREPACK_NPM_PASSWORD}`, `utf8`).toString(`base64`);
    headers.authorization = `Basic ${encodedCreds}`;
  }
  return fetchAsJson(`${npmRegistryUrl}/${packageName}${version2 ? `/${version2}` : ``}`, { headers });
}
function verifySignature({ signatures, integrity, packageName, version: version2 }) {
  if (!Array.isArray(signatures) || !signatures.length) throw new Error(`No compatible signature found in package metadata`);
  const { npm: trustedKeys } = process.env.COREPACK_INTEGRITY_KEYS ? JSON.parse(process.env.COREPACK_INTEGRITY_KEYS) : config_default.keys;
  let signature;
  let key;
  for (const k of trustedKeys) {
    signature = signatures.find(({ keyid }) => keyid === k.keyid);
    if (signature != null) {
      key = k.key;
      break;
    }
  }
  if (signature?.sig == null) throw new import_clipanion2.UsageError(`The package was not signed by any trusted keys: ${JSON.stringify({ signatures, trustedKeys }, void 0, 2)}`);
  const verifier = (0, import_crypto.createVerify)(`SHA256`);
  verifier.end(`${packageName}@${version2}:${integrity}`);
  const valid = verifier.verify(
    `-----BEGIN PUBLIC KEY-----
${key}
-----END PUBLIC KEY-----`,
    signature.sig,
    `base64`
  );
  if (!valid) {
    throw new Error(`Signature does not match`);
  }
}
async function fetchLatestStableVersion(packageName) {
  const metadata = await fetchAsJson2(packageName, `latest`);
  const { version: version2, dist: { integrity, signatures, shasum } } = metadata;
  if (!shouldSkipIntegrityCheck()) {
    try {
      verifySignature({
        packageName,
        version: version2,
        integrity,
        signatures
      });
    } catch (cause) {
      throw new Error(`Corepack cannot download the latest stable version of ${packageName}; you can disable signature verification by setting COREPACK_INTEGRITY_CHECK to 0 in your env, or instruct Corepack to use the latest stable release known by this version of Corepack by setting COREPACK_USE_LATEST to 0`, { cause });
    }
  }
  return `${version2}+${integrity ? `sha512.${Buffer.from(integrity.slice(7), `base64`).toString(`hex`)}` : `sha1.${shasum}`}`;
}
async function fetchAvailableTags(packageName) {
  const metadata = await fetchAsJson2(packageName);
  return metadata[`dist-tags`];
}
async function fetchAvailableVersions(packageName) {
  const metadata = await fetchAsJson2(packageName);
  return Object.keys(metadata.versions);
}
async function fetchTarballURLAndSignature(packageName, version2) {
  const versionMetadata = await fetchAsJson2(packageName, version2);
  const { tarball, signatures, integrity } = versionMetadata.dist;
  if (tarball === void 0 || !tarball.startsWith(`http`))
    throw new Error(`${packageName}@${version2} does not have a valid tarball.`);
  return { tarball, signatures, integrity };
}

// sources/httpUtils.ts
async function fetch(input, init) {
  if (process.env.COREPACK_ENABLE_NETWORK === `0`)
    throw new import_clipanion3.UsageError(`Network access disabled by the environment; can't reach ${input}`);
  if (typeof input === `string`)
    input = new URL(input);
  let headers = init?.headers;
  const username = input.username || process.env.COREPACK_NPM_USERNAME;
  const password = input.password || process.env.COREPACK_NPM_PASSWORD;
  if (username || password) {
    headers = {
      ...headers,
      authorization: `Basic ${Buffer.from(`${username}:${password}`).toString(`base64`)}`
    };
    input.username = input.password = ``;
  }
  const registry = process.env.COREPACK_NPM_TOKEN && new URL(process.env.COREPACK_NPM_REGISTRY || DEFAULT_NPM_REGISTRY_URL);
  if (registry && input.origin === registry.origin) {
    headers = {
      ...headers,
      authorization: `Bearer ${process.env.COREPACK_NPM_TOKEN}`
    };
  }
  let response;
  try {
    response = await globalThis.fetch(input, {
      ...init,
      headers
    });
  } catch (error) {
    throw new Error(
      `Error when performing the request to ${input}; for troubleshooting help, see https://github.com/nodejs/corepack#troubleshooting`,
      { cause: error }
    );
  }
  if (!response.ok) {
    await response.arrayBuffer();
    throw new Error(
      `Server answered with HTTP ${response.status} when performing the request to ${input}; for troubleshooting help, see https://github.com/nodejs/corepack#troubleshooting`
    );
  }
  return response;
}
async function fetchAsJson(input, init) {
  const response = await fetch(input, init);
  return response.json();
}
async function fetchUrlStream(input, init) {
  if (process.env.COREPACK_ENABLE_DOWNLOAD_PROMPT === `1`) {
    console.error(`! Corepack is about to download ${input}`);
    if (import_process2.stdin.isTTY && !process.env.CI) {
      import_process2.stderr.write(`? Do you want to continue? [Y/n] `);
      import_process2.stdin.resume();
      const chars = await (0, import_events.once)(import_process2.stdin, `data`);
      import_process2.stdin.pause();
      if (chars[0][0] === 110 || chars[0][0] === 78)
        throw new import_clipanion3.UsageError(`Aborted by the user`);
      console.error();
    }
  }
  const response = await fetch(input, init);
  const webStream = response.body;
  (0, import_assert.default)(webStream, `Expected stream to be set`);
  const stream = import_stream.Readable.fromWeb(webStream);
  return stream;
}

// sources/nodeUtils.ts
var import_os2 = __toESM(require("os"));
function isNodeError(err) {
  return !!err?.code;
}
function isExistError(err) {
  return err.code === `EEXIST` || err.code === `ENOTEMPTY`;
}
function getEndOfLine(content) {
  const matches = content.match(/\r?\n/g);
  if (matches === null)
    return import_os2.default.EOL;
  const crlf = matches.filter((nl) => nl === `\r
`).length;
  const lf = matches.length - crlf;
  return crlf > lf ? `\r
` : `
`;
}
function normalizeLineEndings(originalContent, newContent) {
  return newContent.replace(/\r?\n/g, getEndOfLine(originalContent));
}
function getIndent(content) {
  const indentMatch = content.match(/^[ \t]+/m);
  if (indentMatch) {
    return indentMatch[0];
  } else {
    return `  `;
  }
}
function stripBOM(content) {
  if (content.charCodeAt(0) === 65279) {
    return content.slice(1);
  } else {
    return content;
  }
}
function readPackageJson(content) {
  return {
    data: JSON.parse(stripBOM(content) || `{}`),
    indent: getIndent(content)
  };
}

// sources/corepackUtils.ts
var YARN_SWITCH_REGEX = /[/\\]switch[/\\]bin[/\\]/;
function isYarnSwitchPath(p) {
  return YARN_SWITCH_REGEX.test(p);
}
function getRegistryFromPackageManagerSpec(spec) {
  return process.env.COREPACK_NPM_REGISTRY ? spec.npmRegistry ?? spec.registry : spec.registry;
}
async function fetchLatestStableVersion2(spec) {
  switch (spec.type) {
    case `npm`: {
      return await fetchLatestStableVersion(spec.package);
    }
    case `url`: {
      const data = await fetchAsJson(spec.url);
      return data[spec.fields.tags].stable;
    }
    default: {
      throw new Error(`Unsupported specification ${JSON.stringify(spec)}`);
    }
  }
}
async function fetchAvailableTags2(spec) {
  switch (spec.type) {
    case `npm`: {
      return await fetchAvailableTags(spec.package);
    }
    case `url`: {
      const data = await fetchAsJson(spec.url);
      return data[spec.fields.tags];
    }
    default: {
      throw new Error(`Unsupported specification ${JSON.stringify(spec)}`);
    }
  }
}
async function fetchAvailableVersions2(spec) {
  switch (spec.type) {
    case `npm`: {
      return await fetchAvailableVersions(spec.package);
    }
    case `url`: {
      const data = await fetchAsJson(spec.url);
      const field = data[spec.fields.versions];
      return Array.isArray(field) ? field : Object.keys(field);
    }
    default: {
      throw new Error(`Unsupported specification ${JSON.stringify(spec)}`);
    }
  }
}
async function findInstalledVersion(installTarget, descriptor) {
  const installFolder = import_path3.default.join(installTarget, descriptor.name);
  let cacheDirectory;
  try {
    cacheDirectory = await import_fs4.default.promises.opendir(installFolder);
  } catch (error) {
    if (isNodeError(error) && error.code === `ENOENT`) {
      return null;
    } else {
      throw error;
    }
  }
  const range = new import_range.default(descriptor.range);
  let bestMatch = null;
  let maxSV = void 0;
  for await (const { name: name2 } of cacheDirectory) {
    if (name2.startsWith(`.`))
      continue;
    if (range.test(name2) && maxSV?.compare(name2) !== 1) {
      bestMatch = name2;
      maxSV = new import_semver.default(bestMatch);
    }
  }
  return bestMatch;
}
function isSupportedPackageManagerDescriptor(descriptor) {
  return !URL.canParse(descriptor.range);
}
function isSupportedPackageManagerLocator(locator) {
  return !URL.canParse(locator.reference);
}
function parseURLReference(locator) {
  const { hash, href } = new URL(locator.reference);
  if (hash) {
    return {
      version: encodeURIComponent(href.slice(0, -hash.length)),
      build: hash.slice(1).split(`.`)
    };
  }
  return { version: encodeURIComponent(href), build: [] };
}
function isValidBinList(x) {
  return Array.isArray(x) && x.length > 0;
}
function isValidBinSpec(x) {
  return typeof x === `object` && x !== null && !Array.isArray(x) && Object.keys(x).length > 0;
}
async function download(installTarget, url, algo, binPath = null) {
  const tmpFolder = getTemporaryFolder(installTarget);
  log(`Downloading to ${tmpFolder}`);
  const stream = await fetchUrlStream(url);
  const parsedUrl = new URL(url);
  const ext = import_path3.default.posix.extname(parsedUrl.pathname);
  let outputFile = null;
  let sendTo;
  if (ext === `.tgz`) {
    const { extract: tarX } = await Promise.resolve().then(() => (init_extract(), extract_exports));
    sendTo = tarX({
      strip: 1,
      cwd: tmpFolder,
      filter: binPath ? (path16) => {
        const pos2 = path16.indexOf(`/`);
        return pos2 !== -1 && path16.slice(pos2 + 1) === binPath;
      } : void 0
    });
  } else if (ext === `.js`) {
    outputFile = import_path3.default.join(tmpFolder, import_path3.default.posix.basename(parsedUrl.pathname));
    sendTo = import_fs4.default.createWriteStream(outputFile);
  }
  stream.pipe(sendTo);
  let hash = !binPath ? stream.pipe((0, import_crypto2.createHash)(algo)) : null;
  await (0, import_events4.once)(sendTo, `finish`);
  if (binPath) {
    const downloadedBin = import_path3.default.join(tmpFolder, binPath);
    outputFile = import_path3.default.join(tmpFolder, import_path3.default.basename(downloadedBin));
    try {
      await renameSafe(downloadedBin, outputFile);
    } catch (err) {
      if (isNodeError(err) && err.code === `ENOENT`)
        throw new Error(`Cannot locate '${binPath}' in downloaded tarball`, { cause: err });
      if (isNodeError(err) && isExistError(err)) {
        await import_fs4.default.promises.rm(downloadedBin);
      } else {
        throw err;
      }
    }
    const fileStream = import_fs4.default.createReadStream(outputFile);
    hash = fileStream.pipe((0, import_crypto2.createHash)(algo));
    await (0, import_events4.once)(fileStream, `close`);
  }
  return {
    tmpFolder,
    outputFile,
    hash: hash.digest(`hex`)
  };
}
async function installVersion(installTarget, locator, { spec }) {
  const locatorIsASupportedPackageManager = isSupportedPackageManagerLocator(locator);
  const locatorReference = locatorIsASupportedPackageManager ? (0, import_parse3.default)(locator.reference) : parseURLReference(locator);
  const { version: version2, build } = locatorReference;
  const installFolder = import_path3.default.join(installTarget, locator.name, version2);
  try {
    const corepackFile = import_path3.default.join(installFolder, `.corepack`);
    const corepackContent = await import_fs4.default.promises.readFile(corepackFile, `utf8`);
    const corepackData = JSON.parse(corepackContent);
    log(`Reusing ${locator.name}@${locator.reference} found in ${installFolder}`);
    return {
      hash: corepackData.hash,
      location: installFolder,
      bin: corepackData.bin
    };
  } catch (err) {
    if (isNodeError(err) && err.code !== `ENOENT`) {
      throw err;
    }
  }
  let url;
  let signatures;
  let integrity;
  let binPath = null;
  if (locatorIsASupportedPackageManager) {
    url = spec.url.replace(`{}`, version2);
    if (process.env.COREPACK_NPM_REGISTRY) {
      const registry = getRegistryFromPackageManagerSpec(spec);
      if (registry.type === `npm`) {
        ({ tarball: url, signatures, integrity } = await fetchTarballURLAndSignature(registry.package, version2));
        if (registry.bin) {
          binPath = registry.bin;
        }
      }
      url = url.replace(
        DEFAULT_NPM_REGISTRY_URL,
        () => process.env.COREPACK_NPM_REGISTRY
      );
    }
  } else {
    url = decodeURIComponent(version2);
    if (process.env.COREPACK_NPM_REGISTRY && url.startsWith(DEFAULT_NPM_REGISTRY_URL)) {
      url = url.replace(
        DEFAULT_NPM_REGISTRY_URL,
        () => process.env.COREPACK_NPM_REGISTRY
      );
    }
  }
  log(`Installing ${locator.name}@${version2} from ${url}`);
  const algo = build[0] ?? `sha512`;
  const { tmpFolder, outputFile, hash: actualHash } = await download(installTarget, url, algo, binPath);
  let bin;
  const isSingleFile = outputFile !== null;
  if (isSingleFile) {
    if (locatorIsASupportedPackageManager && isValidBinList(spec.bin)) {
      bin = spec.bin;
    } else {
      bin = [locator.name];
    }
  } else {
    if (locatorIsASupportedPackageManager && isValidBinSpec(spec.bin)) {
      bin = spec.bin;
    } else {
      const { name: packageName, bin: packageBin } = require(import_path3.default.join(tmpFolder, `package.json`));
      if (typeof packageBin === `string`) {
        bin = { [packageName]: packageBin };
      } else if (isValidBinSpec(packageBin)) {
        bin = packageBin;
      } else {
        throw new Error(`Unable to locate bin in package.json`);
      }
    }
  }
  if (!build[1]) {
    const registry = getRegistryFromPackageManagerSpec(spec);
    if (registry.type === `npm` && !registry.bin && !shouldSkipIntegrityCheck()) {
      if (signatures == null || integrity == null)
        ({ signatures, integrity } = await fetchTarballURLAndSignature(registry.package, version2));
      verifySignature({ signatures, integrity, packageName: registry.package, version: version2 });
      build[1] = Buffer.from(integrity.slice(`sha512-`.length), `base64`).toString(`hex`);
    }
  }
  if (build[1] && actualHash !== build[1])
    throw new Error(`Mismatch hashes. Expected ${build[1]}, got ${actualHash}`);
  const serializedHash = `${algo}.${actualHash}`;
  await import_fs4.default.promises.writeFile(import_path3.default.join(tmpFolder, `.corepack`), JSON.stringify({
    locator,
    bin,
    hash: serializedHash
  }));
  await import_fs4.default.promises.mkdir(import_path3.default.dirname(installFolder), { recursive: true });
  try {
    await renameSafe(tmpFolder, installFolder);
  } catch (err) {
    if (isNodeError(err) && (isExistError(err) || // On Windows the error code is EPERM so we check if it is a directory
    err.code === `EPERM` && (await import_fs4.default.promises.stat(installFolder)).isDirectory())) {
      log(`Another instance of corepack installed ${locator.name}@${locator.reference}`);
      await import_fs4.default.promises.rm(tmpFolder, { recursive: true, force: true });
    } else {
      throw err;
    }
  }
  if (locatorIsASupportedPackageManager && process.env.COREPACK_DEFAULT_TO_LATEST !== `0`) {
    const lastKnownGood = await getLastKnownGood();
    const defaultVersion = getLastKnownGoodFromFileContent(lastKnownGood, locator.name);
    if (defaultVersion) {
      const currentDefault = (0, import_parse3.default)(defaultVersion);
      const downloadedVersion = locatorReference;
      if (currentDefault.major === downloadedVersion.major && (0, import_lt.default)(currentDefault, downloadedVersion)) {
        await activatePackageManager(lastKnownGood, locator);
      }
    }
  }
  log(`Download and install of ${locator.name}@${locator.reference} is finished`);
  return {
    location: installFolder,
    bin,
    hash: serializedHash
  };
}
async function renameSafe(oldPath, newPath) {
  if (process.platform === `win32`) {
    await renameUnderWindows(oldPath, newPath);
  } else {
    await import_fs4.default.promises.rename(oldPath, newPath);
  }
}
async function renameUnderWindows(oldPath, newPath) {
  const retries = 5;
  for (let i = 0; i < retries; i++) {
    try {
      await import_fs4.default.promises.rename(oldPath, newPath);
      break;
    } catch (err) {
      if (isNodeError(err) && (err.code === `ENOENT` || err.code === `EPERM`) && i < retries - 1) {
        await (0, import_promises2.setTimeout)(100 * 2 ** i);
        continue;
      } else {
        throw err;
      }
    }
  }
}
async function runVersion(locator, installSpec, binName, args) {
  let binPath = null;
  const bin = installSpec.bin ?? installSpec.spec.bin;
  if (Array.isArray(bin)) {
    if (bin.some((name2) => name2 === binName)) {
      const parsedUrl = new URL(installSpec.spec.url);
      const ext = import_path3.default.posix.extname(parsedUrl.pathname);
      if (ext === `.js`) {
        binPath = import_path3.default.join(installSpec.location, import_path3.default.posix.basename(parsedUrl.pathname));
      }
    }
  } else {
    for (const [name2, dest] of Object.entries(bin)) {
      if (name2 === binName) {
        binPath = import_path3.default.join(installSpec.location, dest);
        break;
      }
    }
  }
  if (!binPath)
    throw new Error(`Assertion failed: Unable to locate path for bin '${binName}'`);
  if (!import_module.default.enableCompileCache) {
    if (locator.name !== `npm` || (0, import_lt.default)(locator.reference, `9.7.0`)) {
      await Promise.resolve().then(() => __toESM(require_v8_compile_cache()));
    }
  }
  process.env.COREPACK_ROOT = import_path3.default.dirname(require.resolve("corepack/package.json"));
  process.argv = [
    process.execPath,
    binPath,
    ...args
  ];
  process.execArgv = [];
  process.mainModule = void 0;
  process.nextTick(import_module.default.runMain, binPath);
  if (import_module.default.flushCompileCache) {
    setImmediate(import_module.default.flushCompileCache);
  }
}
function shouldSkipIntegrityCheck() {
  return process.env.COREPACK_INTEGRITY_KEYS === `` || process.env.COREPACK_INTEGRITY_KEYS === `0`;
}

// sources/semverUtils.ts
var import_range2 = __toESM(require_range());
var import_semver2 = __toESM(require_semver());
function satisfiesWithPrereleases(version2, range, loose = false) {
  let semverRange;
  try {
    semverRange = new import_range2.default(range, loose);
  } catch (err) {
    return false;
  }
  if (!version2)
    return false;
  let semverVersion;
  try {
    semverVersion = new import_semver2.default(version2, semverRange.loose);
    if (semverVersion.prerelease) {
      semverVersion.prerelease = [];
    }
  } catch (err) {
    return false;
  }
  return semverRange.set.some((comparatorSet) => {
    for (const comparator of comparatorSet)
      if (comparator.semver.prerelease)
        comparator.semver.prerelease = [];
    return comparatorSet.every((comparator) => {
      return comparator.test(semverVersion);
    });
  });
}

// sources/specUtils.ts
var import_clipanion4 = __toESM(require_advanced());
var import_fs5 = __toESM(require("fs"));
var import_path4 = __toESM(require("path"));
var import_satisfies = __toESM(require_satisfies());
var import_valid = __toESM(require_valid());
var import_valid2 = __toESM(require_valid2());
var import_util = require("util");

// sources/types.ts
var SupportedPackageManagers = /* @__PURE__ */ ((SupportedPackageManagers3) => {
  SupportedPackageManagers3["Npm"] = `npm`;
  SupportedPackageManagers3["Pnpm"] = `pnpm`;
  SupportedPackageManagers3["Yarn"] = `yarn`;
  return SupportedPackageManagers3;
})(SupportedPackageManagers || {});
var SupportedPackageManagerSet = new Set(
  Object.values(SupportedPackageManagers)
);
var SupportedPackageManagerSetWithoutNpm = new Set(
  Object.values(SupportedPackageManagers)
);
SupportedPackageManagerSetWithoutNpm.delete("npm" /* Npm */);
function isSupportedPackageManager(value) {
  return SupportedPackageManagerSet.has(value);
}

// sources/specUtils.ts
var nodeModulesRegExp = /[\\/]node_modules[\\/](@[^\\/]*[\\/])?([^@\\/][^\\/]*)$/;
function parseSpec(raw2, source, { enforceExactVersion = true } = {}) {
  if (typeof raw2 !== `string`)
    throw new import_clipanion4.UsageError(`Invalid package manager specification in ${source}; expected a string`);
  const atIndex = raw2.indexOf(`@`);
  if (atIndex === -1 || atIndex === raw2.length - 1) {
    if (enforceExactVersion)
      throw new import_clipanion4.UsageError(`No version specified for ${raw2} in "packageManager" of ${source}`);
    const name3 = atIndex === -1 ? raw2 : raw2.slice(0, -1);
    if (!isSupportedPackageManager(name3))
      throw new import_clipanion4.UsageError(`Unsupported package manager specification (${name3})`);
    return {
      name: name3,
      range: `*`
    };
  }
  const name2 = raw2.slice(0, atIndex);
  const range = raw2.slice(atIndex + 1);
  const isURL = URL.canParse(range);
  if (!isURL) {
    if (enforceExactVersion && !(0, import_valid.default)(range))
      throw new import_clipanion4.UsageError(`Invalid package manager specification in ${source} (${raw2}); expected a semver version${enforceExactVersion ? `` : `, range, or tag`}`);
    if (!isSupportedPackageManager(name2)) {
      throw new import_clipanion4.UsageError(`Unsupported package manager specification (${raw2})`);
    }
  } else if (isSupportedPackageManager(name2) && process.env.COREPACK_ENABLE_UNSAFE_CUSTOM_URLS !== `1`) {
    throw new import_clipanion4.UsageError(`Illegal use of URL for known package manager. Instead, select a specific version, or set COREPACK_ENABLE_UNSAFE_CUSTOM_URLS=1 in your environment (${raw2})`);
  }
  return {
    name: name2,
    range
  };
}
function warnOrThrow(errorMessage, onFail) {
  switch (onFail) {
    case `ignore`:
      break;
    case `error`:
    case void 0:
      throw new import_clipanion4.UsageError(errorMessage);
    default:
      console.warn(`! Corepack validation warning: ${errorMessage}`);
  }
}
function parsePackageJSON(packageJSONContent) {
  const { packageManager: pm } = packageJSONContent;
  if (packageJSONContent.devEngines?.packageManager != null) {
    const { packageManager } = packageJSONContent.devEngines;
    if (typeof packageManager !== `object`) {
      console.warn(`! Corepack only supports objects as valid value for devEngines.packageManager. The current value (${JSON.stringify(packageManager)}) will be ignored.`);
      return pm;
    }
    if (Array.isArray(packageManager)) {
      console.warn(`! Corepack does not currently support array values for devEngines.packageManager`);
      return pm;
    }
    const { name: name2, version: version2, onFail } = packageManager;
    if (typeof name2 !== `string` || name2.includes(`@`)) {
      warnOrThrow(`The value of devEngines.packageManager.name ${JSON.stringify(name2)} is not a supported string value`, onFail);
      return pm;
    }
    if (version2 != null && (typeof version2 !== `string` || !(0, import_valid2.default)(version2))) {
      warnOrThrow(`The value of devEngines.packageManager.version ${JSON.stringify(version2)} is not a valid semver range`, onFail);
      return pm;
    }
    log(`devEngines.packageManager defines that ${name2}@${version2} is the local package manager`);
    if (pm) {
      if (!pm.startsWith?.(`${name2}@`))
        warnOrThrow(`"packageManager" field is set to ${JSON.stringify(pm)} which does not match the "devEngines.packageManager" field set to ${JSON.stringify(name2)}`, onFail);
      else if (version2 != null && !(0, import_satisfies.default)(pm.slice(packageManager.name.length + 1), version2))
        warnOrThrow(`"packageManager" field is set to ${JSON.stringify(pm)} which does not match the value defined in "devEngines.packageManager" for ${JSON.stringify(name2)} of ${JSON.stringify(version2)}`, onFail);
      return pm;
    }
    return `${name2}@${version2 ?? `*`}`;
  }
  return pm;
}
async function setLocalPackageManager(cwd, info) {
  const lookup = await loadSpec(cwd);
  const range = `range` in lookup && lookup.range;
  if (range) {
    if (info.locator.name !== range.name || !(0, import_satisfies.default)(info.locator.reference, range.range)) {
      warnOrThrow(`The requested version of ${info.locator.name}@${info.locator.reference} does not match the devEngines specification (${range.name}@${range.range})`, range.onFail);
    }
  }
  const content = lookup.type !== `NoProject` ? await import_fs5.default.promises.readFile(lookup.target, `utf8`) : ``;
  const { data, indent } = readPackageJson(content);
  const previousPackageManager = data.packageManager ?? (range ? `${range.name}@${range.range}` : `unknown`);
  data.packageManager = `${info.locator.name}@${info.locator.reference}`;
  const newContent = normalizeLineEndings(content, `${JSON.stringify(data, null, indent)}
`);
  await import_fs5.default.promises.writeFile(lookup.target, newContent, `utf8`);
  return {
    previousPackageManager
  };
}
async function loadSpec(initialCwd) {
  let nextCwd = initialCwd;
  let currCwd = ``;
  let selection = null;
  while (nextCwd !== currCwd && (!selection || !selection.data.packageManager)) {
    currCwd = nextCwd;
    nextCwd = import_path4.default.dirname(currCwd);
    if (nodeModulesRegExp.test(currCwd))
      continue;
    const manifestPath = import_path4.default.join(currCwd, `package.json`);
    log(`Checking ${manifestPath}`);
    let content;
    try {
      content = await import_fs5.default.promises.readFile(manifestPath, `utf8`);
    } catch (err) {
      if (err?.code === `ENOENT`) continue;
      throw err;
    }
    let data;
    try {
      data = JSON.parse(content);
    } catch {
    }
    if (typeof data !== `object` || data === null)
      throw new import_clipanion4.UsageError(`Invalid package.json in ${import_path4.default.relative(initialCwd, manifestPath)}`);
    let localEnv;
    const envFilePath2 = import_path4.default.resolve(currCwd, process.env.COREPACK_ENV_FILE ?? `.corepack.env`);
    if (process.env.COREPACK_ENV_FILE == `0`) {
      log(`Skipping env file as configured with COREPACK_ENV_FILE`);
      localEnv = process.env;
    } else if (typeof import_util.parseEnv !== `function`) {
      log(`Skipping env file as it is not supported by the current version of Node.js`);
      localEnv = process.env;
    } else {
      log(`Checking ${envFilePath2}`);
      try {
        localEnv = {
          ...Object.fromEntries(Object.entries((0, import_util.parseEnv)(await import_fs5.default.promises.readFile(envFilePath2, `utf8`))).filter((e) => e[0].startsWith(`COREPACK_`))),
          ...process.env
        };
        log(`Successfully loaded env file found at ${envFilePath2}`);
      } catch (err) {
        if (err?.code !== `ENOENT`)
          throw err;
        log(`No env file found at ${envFilePath2}`);
        localEnv = process.env;
      }
    }
    selection = { data, manifestPath, localEnv, envFilePath: envFilePath2 };
  }
  if (selection === null)
    return { type: `NoProject`, target: import_path4.default.join(initialCwd, `package.json`) };
  let envFilePath;
  if (selection.localEnv !== process.env) {
    envFilePath = selection.envFilePath;
    process.env = selection.localEnv;
  }
  const rawPmSpec = parsePackageJSON(selection.data);
  if (typeof rawPmSpec === `undefined`)
    return { type: `NoSpec`, target: selection.manifestPath };
  log(`${selection.manifestPath} defines ${rawPmSpec} as local package manager`);
  return {
    type: `Found`,
    target: selection.manifestPath,
    envFilePath,
    range: selection.data.devEngines?.packageManager?.version && {
      name: selection.data.devEngines.packageManager.name,
      range: selection.data.devEngines.packageManager.version,
      onFail: selection.data.devEngines.packageManager.onFail
    },
    // Lazy-loading it so we do not throw errors on commands that do not need valid spec.
    getSpec: ({ enforceExactVersion = true } = {}) => parseSpec(rawPmSpec, import_path4.default.relative(initialCwd, selection.manifestPath), { enforceExactVersion })
  };
}

// sources/Engine.ts
function getLastKnownGoodFilePath() {
  const lkg = import_path5.default.join(getCorepackHomeFolder(), `lastKnownGood.json`);
  log(`LastKnownGood file would be located at ${lkg}`);
  return lkg;
}
async function getLastKnownGood() {
  let raw2;
  try {
    raw2 = await import_fs6.default.promises.readFile(getLastKnownGoodFilePath(), `utf8`);
  } catch (err) {
    if (err?.code === `ENOENT`) {
      log(`No LastKnownGood version found in Corepack home.`);
      return {};
    }
    throw err;
  }
  try {
    const parsed = JSON.parse(raw2);
    if (!parsed) {
      log(`Invalid LastKnowGood file in Corepack home (JSON parsable, but falsy)`);
      return {};
    }
    if (typeof parsed !== `object`) {
      log(`Invalid LastKnowGood file in Corepack home (JSON parsable, but non-object)`);
      return {};
    }
    Object.entries(parsed).forEach(([key, value]) => {
      if (typeof value !== `string`) {
        log(`Ignoring key ${key} in LastKnownGood file as its value is not a string`);
        delete parsed[key];
      }
    });
    return parsed;
  } catch {
    log(`Invalid LastKnowGood file in Corepack home (maybe not JSON parsable)`);
    return {};
  }
}
async function createLastKnownGoodFile(lastKnownGood) {
  const content = `${JSON.stringify(lastKnownGood, null, 2)}
`;
  await import_fs6.default.promises.mkdir(getCorepackHomeFolder(), { recursive: true });
  await import_fs6.default.promises.writeFile(getLastKnownGoodFilePath(), content, `utf8`);
}
function getLastKnownGoodFromFileContent(lastKnownGood, packageManager) {
  if (Object.hasOwn(lastKnownGood, packageManager))
    return lastKnownGood[packageManager];
  return void 0;
}
async function activatePackageManager(lastKnownGood, locator) {
  if (lastKnownGood[locator.name] === locator.reference) {
    log(`${locator.name}@${locator.reference} is already Last Known Good version`);
    return;
  }
  lastKnownGood[locator.name] = locator.reference;
  log(`Setting ${locator.name}@${locator.reference} as Last Known Good version`);
  await createLastKnownGoodFile(lastKnownGood);
}
var Engine = class {
  constructor(config = config_default) {
    this.config = config;
  }
  config;
  getPackageManagerFor(binaryName) {
    for (const packageManager of SupportedPackageManagerSet) {
      for (const rangeDefinition of Object.values(this.config.definitions[packageManager].ranges)) {
        const bins = Array.isArray(rangeDefinition.bin) ? rangeDefinition.bin : Object.keys(rangeDefinition.bin);
        if (bins.includes(binaryName)) {
          return packageManager;
        }
      }
    }
    return null;
  }
  getPackageManagerSpecFor(locator) {
    if (!isSupportedPackageManagerLocator(locator)) {
      const url = `${locator.reference}`;
      return {
        url,
        bin: void 0,
        // bin will be set later
        registry: {
          type: `url`,
          url,
          fields: {
            tags: ``,
            versions: ``
          }
        }
      };
    }
    const definition = this.config.definitions[locator.name];
    if (typeof definition === `undefined`)
      throw new import_clipanion5.UsageError(`This package manager (${locator.name}) isn't supported by this corepack build`);
    const ranges = Object.keys(definition.ranges).reverse();
    const range = ranges.find((range2) => satisfiesWithPrereleases(locator.reference, range2));
    if (typeof range === `undefined`)
      throw new Error(`Assertion failed: Specified resolution (${locator.reference}) isn't supported by any of ${ranges.join(`, `)}`);
    return definition.ranges[range];
  }
  getBinariesFor(name2) {
    const binNames = /* @__PURE__ */ new Set();
    for (const rangeDefinition of Object.values(this.config.definitions[name2].ranges)) {
      const bins = Array.isArray(rangeDefinition.bin) ? rangeDefinition.bin : Object.keys(rangeDefinition.bin);
      for (const name3 of bins) {
        binNames.add(name3);
      }
    }
    return binNames;
  }
  async getDefaultDescriptors() {
    const locators = [];
    for (const name2 of SupportedPackageManagerSet)
      locators.push({ name: name2, range: await this.getDefaultVersion(name2) });
    return locators;
  }
  async getDefaultVersion(packageManager) {
    const definition = this.config.definitions[packageManager];
    if (typeof definition === `undefined`)
      throw new import_clipanion5.UsageError(`This package manager (${packageManager}) isn't supported by this corepack build`);
    const lastKnownGood = await getLastKnownGood();
    const lastKnownGoodForThisPackageManager = getLastKnownGoodFromFileContent(lastKnownGood, packageManager);
    if (lastKnownGoodForThisPackageManager) {
      log(`Search for default version: Found ${packageManager}@${lastKnownGoodForThisPackageManager} in LastKnownGood file`);
      return lastKnownGoodForThisPackageManager;
    }
    if (import_process3.default.env.COREPACK_DEFAULT_TO_LATEST === `0`) {
      log(`Search for default version: As defined in environment, defaulting to internal config ${packageManager}@${definition.default}`);
      return definition.default;
    }
    const reference = await fetchLatestStableVersion2(definition.fetchLatestFrom);
    log(`Search for default version: found in remote registry ${packageManager}@${reference}`);
    try {
      await activatePackageManager(lastKnownGood, {
        name: packageManager,
        reference
      });
    } catch {
      log(`Search for default version: could not activate registry version`);
    }
    return reference;
  }
  async activatePackageManager(locator) {
    const lastKnownGood = await getLastKnownGood();
    await activatePackageManager(lastKnownGood, locator);
  }
  async ensurePackageManager(locator) {
    const spec = this.getPackageManagerSpecFor(locator);
    const packageManagerInfo = await installVersion(getInstallFolder(), locator, {
      spec
    });
    const noHashReference = locator.reference.replace(/\+.*/, ``);
    const fixedHashReference = `${noHashReference}+${packageManagerInfo.hash}`;
    const fixedHashLocator = {
      name: locator.name,
      reference: fixedHashReference
    };
    return {
      ...packageManagerInfo,
      locator: fixedHashLocator,
      spec
    };
  }
  /**
   * Locates the active project's package manager specification.
   *
   * If the specification exists but doesn't match the active package manager,
   * an error is thrown to prevent users from using the wrong package manager,
   * which would lead to inconsistent project layouts.
   *
   * If the project doesn't include a specification file, we just assume that
   * whatever the user uses is exactly what they want to use. Since the version
   * isn't specified, we fallback on known good versions.
   *
   * Finally, if the project doesn't exist at all, we ask the user whether they
   * want to create one in the current project. If they do, we initialize a new
   * project using the default package managers, and configure it so that we
   * don't need to ask again in the future.
   */
  async findProjectSpec(initialCwd, locator, { transparent = false, binaryVersion } = {}) {
    const fallbackDescriptor = { name: locator.name, range: `${locator.reference}` };
    if (import_process3.default.env.COREPACK_ENABLE_PROJECT_SPEC === `0`) {
      if (typeof locator.reference === `function`)
        fallbackDescriptor.range = await locator.reference();
      return fallbackDescriptor;
    }
    if (import_process3.default.env.COREPACK_ENABLE_STRICT === `0`)
      transparent = true;
    while (true) {
      const result = await loadSpec(initialCwd);
      switch (result.type) {
        case `NoProject`: {
          if (typeof locator.reference === `function`)
            fallbackDescriptor.range = await locator.reference();
          log(`Falling back to ${fallbackDescriptor.name}@${fallbackDescriptor.range} as no project manifest were found`);
          return fallbackDescriptor;
        }
        case `NoSpec`: {
          if (typeof locator.reference === `function`)
            fallbackDescriptor.range = await locator.reference();
          if (import_process3.default.env.COREPACK_ENABLE_AUTO_PIN === `1`) {
            const resolved = await this.resolveDescriptor(fallbackDescriptor, { allowTags: true });
            if (resolved === null)
              throw new import_clipanion5.UsageError(`Failed to successfully resolve '${fallbackDescriptor.range}' to a valid ${fallbackDescriptor.name} release`);
            const installSpec = await this.ensurePackageManager(resolved);
            console.error(`! The local project doesn't define a 'packageManager' field. Corepack will now add one referencing ${installSpec.locator.name}@${installSpec.locator.reference}.`);
            console.error(`! For more details about this field, consult the documentation at https://nodejs.org/api/packages.html#packagemanager`);
            console.error();
            await setLocalPackageManager(import_path5.default.dirname(result.target), installSpec);
          }
          log(`Falling back to ${fallbackDescriptor.name}@${fallbackDescriptor.range} in the absence of "packageManager" field in ${result.target}`);
          return fallbackDescriptor;
        }
        case `Found`: {
          const spec = result.getSpec({ enforceExactVersion: !binaryVersion });
          if (spec.name !== locator.name) {
            if (transparent) {
              if (typeof locator.reference === `function`)
                fallbackDescriptor.range = await locator.reference();
              log(`Falling back to ${fallbackDescriptor.name}@${fallbackDescriptor.range} in a ${spec.name}@${spec.range} project`);
              return fallbackDescriptor;
            } else {
              throw new import_clipanion5.UsageError(`This project is configured to use ${spec.name} because ${result.target} has a "packageManager" field`);
            }
          } else {
            log(`Using ${spec.name}@${spec.range} as defined in project manifest ${result.target}`);
            return spec;
          }
        }
      }
    }
  }
  async executePackageManagerRequest({ packageManager, binaryName, binaryVersion }, { cwd, args }) {
    let fallbackLocator = {
      name: binaryName,
      reference: void 0
    };
    let isTransparentCommand = false;
    if (packageManager != null) {
      const defaultVersion = binaryVersion || (() => this.getDefaultVersion(packageManager));
      const definition = this.config.definitions[packageManager];
      for (const transparentPath of definition.transparent.commands) {
        if (transparentPath[0] === binaryName && transparentPath.slice(1).every((segment, index) => segment === args[index])) {
          isTransparentCommand = true;
          break;
        }
      }
      const fallbackReference = isTransparentCommand ? definition.transparent.default ?? defaultVersion : defaultVersion;
      fallbackLocator = {
        name: packageManager,
        reference: fallbackReference
      };
    }
    const descriptor = await this.findProjectSpec(cwd, fallbackLocator, { transparent: isTransparentCommand, binaryVersion });
    if (binaryVersion)
      descriptor.range = binaryVersion;
    const resolved = await this.resolveDescriptor(descriptor, { allowTags: true });
    if (resolved === null)
      throw new import_clipanion5.UsageError(`Failed to successfully resolve '${descriptor.range}' to a valid ${descriptor.name} release`);
    const installSpec = await this.ensurePackageManager(resolved);
    return await runVersion(resolved, installSpec, binaryName, args);
  }
  async resolveDescriptor(descriptor, { allowTags = false, useCache = true } = {}) {
    if (!isSupportedPackageManagerDescriptor(descriptor)) {
      if (import_process3.default.env.COREPACK_ENABLE_UNSAFE_CUSTOM_URLS !== `1` && isSupportedPackageManager(descriptor.name))
        throw new import_clipanion5.UsageError(`Illegal use of URL for known package manager. Instead, select a specific version, or set COREPACK_ENABLE_UNSAFE_CUSTOM_URLS=1 in your environment (${descriptor.name}@${descriptor.range})`);
      return {
        name: descriptor.name,
        reference: descriptor.range
      };
    }
    const definition = this.config.definitions[descriptor.name];
    if (typeof definition === `undefined`)
      throw new import_clipanion5.UsageError(`This package manager (${descriptor.name}) isn't supported by this corepack build`);
    let finalDescriptor = descriptor;
    if (!(0, import_valid3.default)(descriptor.range) && !(0, import_valid4.default)(descriptor.range)) {
      if (!allowTags)
        throw new import_clipanion5.UsageError(`Packages managers can't be referenced via tags in this context`);
      const ranges = Object.keys(definition.ranges);
      const tagRange = ranges[ranges.length - 1];
      const packageManagerSpec = definition.ranges[tagRange];
      const registry = getRegistryFromPackageManagerSpec(packageManagerSpec);
      const tags = await fetchAvailableTags2(registry);
      if (!Object.hasOwn(tags, descriptor.range))
        throw new import_clipanion5.UsageError(`Tag not found (${descriptor.range})`);
      finalDescriptor = {
        name: descriptor.name,
        range: tags[descriptor.range]
      };
    }
    const cachedVersion = await findInstalledVersion(getInstallFolder(), finalDescriptor);
    if (cachedVersion !== null && useCache)
      return { name: finalDescriptor.name, reference: cachedVersion };
    if ((0, import_valid3.default)(finalDescriptor.range))
      return { name: finalDescriptor.name, reference: finalDescriptor.range };
    const versions = await Promise.all(Object.keys(definition.ranges).map(async (range) => {
      const packageManagerSpec = definition.ranges[range];
      const registry = getRegistryFromPackageManagerSpec(packageManagerSpec);
      const versions2 = await fetchAvailableVersions2(registry);
      return versions2.filter((version2) => satisfiesWithPrereleases(version2, finalDescriptor.range));
    }));
    const highestVersion = [...new Set(versions.flat())].sort(import_rcompare.default);
    if (highestVersion.length === 0)
      return null;
    return { name: finalDescriptor.name, reference: highestVersion[0] };
  }
};

// sources/commands/Cache.ts
var import_clipanion6 = __toESM(require_advanced());
var import_fs7 = __toESM(require("fs"));
var CacheCommand = class extends import_clipanion6.Command {
  static paths = [
    [`cache`, `clean`],
    [`cache`, `clear`]
  ];
  static usage = import_clipanion6.Command.Usage({
    description: `Cleans Corepack cache`,
    details: `
      Removes Corepack cache directory from your local disk.
    `
  });
  async execute() {
    await import_fs7.default.promises.rm(getInstallFolder(), { recursive: true, force: true });
  }
};

// sources/commands/Disable.ts
var import_clipanion7 = __toESM(require_advanced());
var import_fs8 = __toESM(require("fs"));
var import_path6 = __toESM(require("path"));
var import_which = __toESM(require_lib());
var DisableCommand = class extends import_clipanion7.Command {
  static paths = [
    [`disable`]
  ];
  static usage = import_clipanion7.Command.Usage({
    description: `Remove the Corepack shims from the install directory`,
    details: `
      When run, this command will remove the shims for the specified package managers from the install directory, or all shims if no parameters are passed.

      By default it will locate the install directory by running the equivalent of \`which corepack\`, but this can be tweaked by explicitly passing the install directory via the \`--install-directory\` flag.
    `,
    examples: [[
      `Disable all shims, removing them if they're next to the \`coreshim\` binary`,
      `$0 disable`
    ], [
      `Disable all shims, removing them from the specified directory`,
      `$0 disable --install-directory /path/to/bin`
    ], [
      `Disable the Yarn shim only`,
      `$0 disable yarn`
    ]]
  });
  installDirectory = import_clipanion7.Option.String(`--install-directory`, {
    description: `Where the shims are located`
  });
  names = import_clipanion7.Option.Rest();
  async execute() {
    let installDirectory = this.installDirectory;
    if (typeof installDirectory === `undefined`)
      installDirectory = import_path6.default.dirname(await (0, import_which.default)(`corepack`));
    const names = this.names.length === 0 ? SupportedPackageManagerSetWithoutNpm : this.names;
    const allBinNames = [];
    for (const name2 of new Set(names)) {
      if (!isSupportedPackageManager(name2))
        throw new import_clipanion7.UsageError(`Invalid package manager name '${name2}'`);
      const binNames = this.context.engine.getBinariesFor(name2);
      allBinNames.push(...binNames);
    }
    const removeLink = process.platform === `win32` ? (binName) => this.removeWin32Link(installDirectory, binName) : (binName) => this.removePosixLink(installDirectory, binName);
    await Promise.all(allBinNames.map(removeLink));
  }
  async removePosixLink(installDirectory, binName) {
    const file = import_path6.default.join(installDirectory, binName);
    try {
      if (binName.includes(`yarn`) && isYarnSwitchPath(await import_fs8.default.promises.realpath(file))) {
        console.warn(`${binName} is already installed in ${file} and points to a Yarn Switch install - skipping`);
        return;
      }
      await import_fs8.default.promises.unlink(file);
    } catch (err) {
      if (err.code !== `ENOENT`) {
        throw err;
      }
    }
  }
  async removeWin32Link(installDirectory, binName) {
    for (const ext of [``, `.ps1`, `.cmd`]) {
      const file = import_path6.default.join(installDirectory, `${binName}${ext}`);
      try {
        await import_fs8.default.promises.unlink(file);
      } catch (err) {
        if (err.code !== `ENOENT`) {
          throw err;
        }
      }
    }
  }
};

// sources/commands/Enable.ts
var import_cmd_shim = __toESM(require_cmd_shim());
var import_clipanion8 = __toESM(require_advanced());
var import_fs9 = __toESM(require("fs"));
var import_path7 = __toESM(require("path"));
var import_which2 = __toESM(require_lib());
var EnableCommand = class extends import_clipanion8.Command {
  static paths = [
    [`enable`]
  ];
  static usage = import_clipanion8.Command.Usage({
    description: `Add the Corepack shims to the install directory`,
    details: `
      When run, this command will check whether the shims for the specified package managers can be found with the correct values inside the install directory. If not, or if they don't exist, they will be created.

      By default it will locate the install directory by running the equivalent of \`which corepack\`, but this can be tweaked by explicitly passing the install directory via the \`--install-directory\` flag.
    `,
    examples: [[
      `Enable all shims, putting them next to the \`corepack\` binary`,
      `$0 enable`
    ], [
      `Enable all shims, putting them in the specified directory`,
      `$0 enable --install-directory /path/to/folder`
    ], [
      `Enable the Yarn shim only`,
      `$0 enable yarn`
    ]]
  });
  installDirectory = import_clipanion8.Option.String(`--install-directory`, {
    description: `Where the shims are to be installed`
  });
  names = import_clipanion8.Option.Rest();
  async execute() {
    let installDirectory = this.installDirectory;
    if (typeof installDirectory === `undefined`)
      installDirectory = import_path7.default.dirname(await (0, import_which2.default)(`corepack`));
    installDirectory = import_fs9.default.realpathSync(installDirectory);
    const manifestPath = require.resolve("corepack/package.json");
    const distFolder = import_path7.default.join(import_path7.default.dirname(manifestPath), `dist`);
    if (!import_fs9.default.existsSync(distFolder))
      throw new Error(`Assertion failed: The stub folder doesn't exist`);
    const names = this.names.length === 0 ? SupportedPackageManagerSetWithoutNpm : this.names;
    const allBinNames = [];
    for (const name2 of new Set(names)) {
      if (!isSupportedPackageManager(name2))
        throw new import_clipanion8.UsageError(`Invalid package manager name '${name2}'`);
      const binNames = this.context.engine.getBinariesFor(name2);
      allBinNames.push(...binNames);
    }
    const generateLink = process.platform === `win32` ? (binName) => this.generateWin32Link(installDirectory, distFolder, binName) : (binName) => this.generatePosixLink(installDirectory, distFolder, binName);
    await Promise.all(allBinNames.map(generateLink));
  }
  async generatePosixLink(installDirectory, distFolder, binName) {
    const file = import_path7.default.join(installDirectory, binName);
    const symlink = import_path7.default.relative(installDirectory, import_path7.default.join(distFolder, `${binName}.js`));
    const stats = import_fs9.default.lstatSync(file, { throwIfNoEntry: false });
    if (stats) {
      if (stats.isSymbolicLink()) {
        const currentSymlink = await import_fs9.default.promises.readlink(file);
        if (binName.includes(`yarn`) && isYarnSwitchPath(await import_fs9.default.promises.realpath(file))) {
          console.warn(`${binName} is already installed in ${file} and points to a Yarn Switch install - skipping`);
          return;
        }
        if (currentSymlink === symlink) {
          return;
        }
      }
      await import_fs9.default.promises.unlink(file);
    }
    await import_fs9.default.promises.symlink(symlink, file);
  }
  async generateWin32Link(installDirectory, distFolder, binName) {
    const file = import_path7.default.join(installDirectory, binName);
    await (0, import_cmd_shim.default)(import_path7.default.join(distFolder, `${binName}.js`), file, {
      createCmdFile: true
    });
  }
};

// sources/commands/InstallGlobal.ts
var import_clipanion10 = __toESM(require_advanced());
var import_fs10 = __toESM(require("fs"));
var import_path8 = __toESM(require("path"));

// sources/commands/Base.ts
var import_clipanion9 = __toESM(require_advanced());
var BaseCommand = class extends import_clipanion9.Command {
  async resolvePatternsToDescriptors({ patterns }) {
    const resolvedSpecs = patterns.map((pattern) => parseSpec(pattern, `CLI arguments`, { enforceExactVersion: false }));
    if (resolvedSpecs.length === 0) {
      const lookup = await loadSpec(this.context.cwd);
      switch (lookup.type) {
        case `NoProject`:
          throw new import_clipanion9.UsageError(`Couldn't find a project in the local directory - please specify the package manager to pack, or run this command from a valid project`);
        case `NoSpec`:
          throw new import_clipanion9.UsageError(`The local project doesn't feature a 'packageManager' field nor a 'devEngines.packageManager' field - please specify the package manager to pack, or update the manifest to reference it`);
        default: {
          return [lookup.range ?? lookup.getSpec()];
        }
      }
    }
    return resolvedSpecs;
  }
  async setAndInstallLocalPackageManager(info) {
    const {
      previousPackageManager
    } = await setLocalPackageManager(this.context.cwd, info);
    const command = this.context.engine.getPackageManagerSpecFor(info.locator).commands?.use ?? null;
    if (command === null)
      return 0;
    process.env.COREPACK_MIGRATE_FROM = previousPackageManager;
    this.context.stdout.write(`
`);
    const [binaryName, ...args] = command;
    return await runVersion(info.locator, info, binaryName, args);
  }
};

// sources/commands/InstallGlobal.ts
var InstallGlobalCommand = class extends BaseCommand {
  static paths = [
    [`install`]
  ];
  static usage = import_clipanion10.Command.Usage({
    description: `Install package managers on the system`,
    details: `
      Download the selected package managers and install them on the system.

      Package managers thus installed will be configured as the new default when calling their respective binaries outside of projects defining the 'packageManager' field.
    `,
    examples: [[
      `Install the latest version of Yarn 1.x and make it globally available`,
      `corepack install -g yarn@^1`
    ], [
      `Install the latest version of pnpm, and make it globally available`,
      `corepack install -g pnpm`
    ]]
  });
  global = import_clipanion10.Option.Boolean(`-g,--global`, {
    required: true
  });
  cacheOnly = import_clipanion10.Option.Boolean(`--cache-only`, false, {
    description: `If true, the package managers will only be cached, not set as new defaults`
  });
  args = import_clipanion10.Option.Rest();
  async execute() {
    if (this.args.length === 0)
      throw new import_clipanion10.UsageError(`No package managers specified`);
    await Promise.all(this.args.map((arg) => {
      if (arg.endsWith(`.tgz`)) {
        return this.installFromTarball(import_path8.default.resolve(this.context.cwd, arg));
      } else {
        return this.installFromDescriptor(parseSpec(arg, `CLI arguments`, { enforceExactVersion: false }));
      }
    }));
  }
  log(locator) {
    if (this.cacheOnly) {
      this.context.stdout.write(`Adding ${locator.name}@${locator.reference} to the cache...
`);
    } else {
      this.context.stdout.write(`Installing ${locator.name}@${locator.reference}...
`);
    }
  }
  async installFromDescriptor(descriptor) {
    const resolved = await this.context.engine.resolveDescriptor(descriptor, { allowTags: true, useCache: false });
    if (resolved === null)
      throw new import_clipanion10.UsageError(`Failed to successfully resolve '${descriptor.range}' to a valid ${descriptor.name} release`);
    this.log(resolved);
    await this.context.engine.ensurePackageManager(resolved);
    if (!this.cacheOnly) {
      await this.context.engine.activatePackageManager(resolved);
    }
  }
  async installFromTarball(p) {
    const installFolder = getInstallFolder();
    const archiveEntries = /* @__PURE__ */ new Map();
    const { list: tarT } = await Promise.resolve().then(() => (init_list(), list_exports));
    let hasShortEntries = false;
    await tarT({ file: p, onentry: (entry) => {
      const segments = entry.path.split(/\//g);
      if (segments.length > 0 && segments[segments.length - 1] !== `.corepack`)
        return;
      if (segments.length < 3) {
        hasShortEntries = true;
      } else {
        let references = archiveEntries.get(segments[0]);
        if (typeof references === `undefined`)
          archiveEntries.set(segments[0], references = /* @__PURE__ */ new Set());
        references.add(segments[1]);
      }
    } });
    if (hasShortEntries || archiveEntries.size < 1)
      throw new import_clipanion10.UsageError(`Invalid archive format; did it get generated by 'corepack pack'?`);
    const { extract: tarX } = await Promise.resolve().then(() => (init_extract(), extract_exports));
    for (const [name2, references] of archiveEntries) {
      for (const reference of references) {
        if (!isSupportedPackageManager(name2))
          throw new import_clipanion10.UsageError(`Unsupported package manager '${name2}'`);
        this.log({ name: name2, reference });
        await import_fs10.default.promises.mkdir(installFolder, { recursive: true });
        await tarX({ file: p, cwd: installFolder }, [`${name2}/${reference}`]);
        if (!this.cacheOnly) {
          await this.context.engine.activatePackageManager({ name: name2, reference });
        }
      }
    }
  }
};

// sources/commands/InstallLocal.ts
var import_clipanion11 = __toESM(require_advanced());
var InstallLocalCommand = class extends BaseCommand {
  static paths = [
    [`install`]
  ];
  static usage = import_clipanion11.Command.Usage({
    description: `Install the package manager configured in the local project`,
    details: `
      Download and install the package manager configured in the local project. This command doesn't change the global version used when running the package manager from outside the project (use the \`-g,--global\` flag if you wish to do this).
    `,
    examples: [[
      `Install the project's package manager in the cache`,
      `corepack install`
    ]]
  });
  async execute() {
    const [descriptor] = await this.resolvePatternsToDescriptors({
      patterns: []
    });
    const resolved = await this.context.engine.resolveDescriptor(descriptor, { allowTags: true });
    if (resolved === null)
      throw new import_clipanion11.UsageError(`Failed to successfully resolve '${descriptor.range}' to a valid ${descriptor.name} release`);
    this.context.stdout.write(`Adding ${resolved.name}@${resolved.reference} to the cache...
`);
    await this.context.engine.ensurePackageManager(resolved);
  }
};

// sources/commands/Pack.ts
var import_clipanion12 = __toESM(require_advanced());
var import_promises3 = require("fs/promises");
var import_path11 = __toESM(require("path"));
var PackCommand = class extends BaseCommand {
  static paths = [
    [`pack`]
  ];
  static usage = import_clipanion12.Command.Usage({
    description: `Store package managers in a tarball`,
    details: `
      Download the selected package managers and store them inside a tarball suitable for use with \`corepack install -g\`.
    `,
    examples: [[
      `Pack the package manager defined in the package.json file`,
      `corepack pack`
    ], [
      `Pack the latest version of Yarn 1.x inside a file named corepack.tgz`,
      `corepack pack yarn@^1`
    ]]
  });
  json = import_clipanion12.Option.Boolean(`--json`, false, {
    description: `If true, the path to the generated tarball will be printed on stdout`
  });
  output = import_clipanion12.Option.String(`-o,--output`, {
    description: `Where the tarball should be generated; by default "corepack.tgz"`
  });
  patterns = import_clipanion12.Option.Rest();
  async execute() {
    const descriptors = await this.resolvePatternsToDescriptors({
      patterns: this.patterns
    });
    const installLocations = [];
    for (const descriptor of descriptors) {
      const resolved = await this.context.engine.resolveDescriptor(descriptor, { allowTags: true, useCache: false });
      if (resolved === null)
        throw new import_clipanion12.UsageError(`Failed to successfully resolve '${descriptor.range}' to a valid ${descriptor.name} release`);
      this.context.stdout.write(`Adding ${resolved.name}@${resolved.reference} to the cache...
`);
      const packageManagerInfo = await this.context.engine.ensurePackageManager(resolved);
      await this.context.engine.activatePackageManager(packageManagerInfo.locator);
      installLocations.push(packageManagerInfo.location);
    }
    const baseInstallFolder = getInstallFolder();
    const outputPath = import_path11.default.resolve(this.context.cwd, this.output ?? `corepack.tgz`);
    if (!this.json) {
      this.context.stdout.write(`
`);
      this.context.stdout.write(`Packing the selected tools in ${import_path11.default.basename(outputPath)}...
`);
    }
    const { create: tarC } = await Promise.resolve().then(() => (init_create(), create_exports));
    await (0, import_promises3.mkdir)(baseInstallFolder, { recursive: true });
    await tarC({ gzip: true, cwd: baseInstallFolder, file: import_path11.default.resolve(outputPath) }, installLocations.map((location) => {
      return import_path11.default.relative(baseInstallFolder, location);
    }));
    if (this.json) {
      this.context.stdout.write(`${JSON.stringify(outputPath)}
`);
    } else {
      this.context.stdout.write(`All done!
`);
    }
  }
};

// sources/commands/Up.ts
var import_clipanion13 = __toESM(require_advanced());
var import_major = __toESM(require_major());
var import_valid5 = __toESM(require_valid());
var import_valid6 = __toESM(require_valid2());
var UpCommand = class extends BaseCommand {
  static paths = [
    [`up`]
  ];
  static usage = import_clipanion13.Command.Usage({
    description: `Update the package manager used in the current project`,
    details: `
      Retrieve the latest available version for the current major release line
      of the package manager used in the local project, and update the project
      to use it.

      Unlike \`corepack use\` this command doesn't take a package manager name
      nor a version range, as it will always select the latest available
      version from the same major line. Should you need to upgrade to a new
      major, use an explicit \`corepack use '{name}@*'\` call.
    `,
    examples: [[
      `Configure the project to use the latest Yarn release`,
      `corepack up`
    ]]
  });
  async execute() {
    const [descriptor] = await this.resolvePatternsToDescriptors({
      patterns: []
    });
    if (!(0, import_valid5.default)(descriptor.range) && !(0, import_valid6.default)(descriptor.range))
      throw new import_clipanion13.UsageError(`The 'corepack up' command can only be used when your project's packageManager field is set to a semver version or semver range`);
    const resolved = await this.context.engine.resolveDescriptor(descriptor, { useCache: false });
    if (!resolved)
      throw new import_clipanion13.UsageError(`Failed to successfully resolve '${descriptor.range}' to a valid ${descriptor.name} release`);
    const majorVersion = (0, import_major.default)(resolved.reference);
    const majorDescriptor = { name: descriptor.name, range: `^${majorVersion}.0.0` };
    const highestVersion = await this.context.engine.resolveDescriptor(majorDescriptor, { useCache: false });
    if (!highestVersion)
      throw new import_clipanion13.UsageError(`Failed to find the highest release for ${descriptor.name} ${majorVersion}.x`);
    this.context.stdout.write(`Installing ${highestVersion.name}@${highestVersion.reference} in the project...
`);
    const packageManagerInfo = await this.context.engine.ensurePackageManager(highestVersion);
    await this.setAndInstallLocalPackageManager(packageManagerInfo);
  }
};

// sources/commands/Use.ts
var import_clipanion14 = __toESM(require_advanced());
var UseCommand = class extends BaseCommand {
  static paths = [
    [`use`]
  ];
  static usage = import_clipanion14.Command.Usage({
    description: `Define the package manager to use for the current project`,
    details: `
      When run, this command will retrieve the latest release matching the
      provided descriptor, assign it to the project's package.json file, and
      automatically perform an install.
    `,
    examples: [[
      `Configure the project to use the latest Yarn release`,
      `corepack use yarn`
    ]]
  });
  pattern = import_clipanion14.Option.String();
  async execute() {
    const [descriptor] = await this.resolvePatternsToDescriptors({
      patterns: [this.pattern]
    });
    const resolved = await this.context.engine.resolveDescriptor(descriptor, { allowTags: true, useCache: false });
    if (resolved === null)
      throw new import_clipanion14.UsageError(`Failed to successfully resolve '${descriptor.range}' to a valid ${descriptor.name} release`);
    this.context.stdout.write(`Installing ${resolved.name}@${resolved.reference} in the project...
`);
    const packageManagerInfo = await this.context.engine.ensurePackageManager(resolved);
    await this.setAndInstallLocalPackageManager(packageManagerInfo);
  }
};

// sources/commands/deprecated/Hydrate.ts
var import_clipanion15 = __toESM(require_advanced());
var import_promises4 = require("fs/promises");
var import_path12 = __toESM(require("path"));
var HydrateCommand = class extends import_clipanion15.Command {
  static paths = [
    [`hydrate`]
  ];
  activate = import_clipanion15.Option.Boolean(`--activate`, false, {
    description: `If true, this release will become the default one for this package manager`
  });
  fileName = import_clipanion15.Option.String();
  async execute() {
    const installFolder = getInstallFolder();
    const fileName = import_path12.default.resolve(this.context.cwd, this.fileName);
    const archiveEntries = /* @__PURE__ */ new Map();
    let hasShortEntries = false;
    const { list: tarT } = await Promise.resolve().then(() => (init_list(), list_exports));
    await tarT({ file: fileName, onentry: (entry) => {
      const segments = entry.path.split(/\//g);
      if (segments.length < 3) {
        hasShortEntries = true;
      } else {
        let references = archiveEntries.get(segments[0]);
        if (typeof references === `undefined`)
          archiveEntries.set(segments[0], references = /* @__PURE__ */ new Set());
        references.add(segments[1]);
      }
    } });
    if (hasShortEntries || archiveEntries.size < 1)
      throw new import_clipanion15.UsageError(`Invalid archive format; did it get generated by 'corepack prepare'?`);
    const { extract: tarX } = await Promise.resolve().then(() => (init_extract(), extract_exports));
    for (const [name2, references] of archiveEntries) {
      for (const reference of references) {
        if (!isSupportedPackageManager(name2))
          throw new import_clipanion15.UsageError(`Unsupported package manager '${name2}'`);
        if (this.activate)
          this.context.stdout.write(`Hydrating ${name2}@${reference} for immediate activation...
`);
        else
          this.context.stdout.write(`Hydrating ${name2}@${reference}...
`);
        await (0, import_promises4.mkdir)(installFolder, { recursive: true });
        await tarX({ file: fileName, cwd: installFolder }, [`${name2}/${reference}`]);
        if (this.activate) {
          await this.context.engine.activatePackageManager({ name: name2, reference });
        }
      }
    }
    this.context.stdout.write(`All done!
`);
  }
};

// sources/commands/deprecated/Prepare.ts
var import_clipanion16 = __toESM(require_advanced());
var import_promises5 = require("fs/promises");
var import_path13 = __toESM(require("path"));
var PrepareCommand = class extends import_clipanion16.Command {
  static paths = [
    [`prepare`]
  ];
  activate = import_clipanion16.Option.Boolean(`--activate`, false, {
    description: `If true, this release will become the default one for this package manager`
  });
  json = import_clipanion16.Option.Boolean(`--json`, false, {
    description: `If true, the output will be the path of the generated tarball`
  });
  output = import_clipanion16.Option.String(`-o,--output`, {
    description: `If true, the installed package managers will also be stored in a tarball`,
    tolerateBoolean: true
  });
  specs = import_clipanion16.Option.Rest();
  async execute() {
    const specs = this.specs;
    const installLocations = [];
    if (specs.length === 0) {
      const lookup = await loadSpec(this.context.cwd);
      switch (lookup.type) {
        case `NoProject`:
          throw new import_clipanion16.UsageError(`Couldn't find a project in the local directory - please specify the package manager to pack, or run this command from a valid project`);
        case `NoSpec`:
          throw new import_clipanion16.UsageError(`The local project doesn't feature a 'packageManager' field - please specify the package manager to pack, or update the manifest to reference it`);
        default: {
          specs.push(lookup.getSpec());
        }
      }
    }
    for (const request of specs) {
      const spec = typeof request === `string` ? parseSpec(request, `CLI arguments`, { enforceExactVersion: false }) : request;
      const resolved = await this.context.engine.resolveDescriptor(spec, { allowTags: true });
      if (resolved === null)
        throw new import_clipanion16.UsageError(`Failed to successfully resolve '${spec.range}' to a valid ${spec.name} release`);
      if (!this.json) {
        if (this.activate) {
          this.context.stdout.write(`Preparing ${spec.name}@${spec.range} for immediate activation...
`);
        } else {
          this.context.stdout.write(`Preparing ${spec.name}@${spec.range}...
`);
        }
      }
      const installSpec = await this.context.engine.ensurePackageManager(resolved);
      installLocations.push(installSpec.location);
      if (this.activate) {
        await this.context.engine.activatePackageManager(resolved);
      }
    }
    if (this.output) {
      const outputName = typeof this.output === `string` ? this.output : `corepack.tgz`;
      const baseInstallFolder = getInstallFolder();
      const outputPath = import_path13.default.resolve(this.context.cwd, outputName);
      if (!this.json)
        this.context.stdout.write(`Packing the selected tools in ${import_path13.default.basename(outputPath)}...
`);
      const { create: tarC } = await Promise.resolve().then(() => (init_create(), create_exports));
      await (0, import_promises5.mkdir)(baseInstallFolder, { recursive: true });
      await tarC({ gzip: true, cwd: baseInstallFolder, file: import_path13.default.resolve(outputPath) }, installLocations.map((location) => {
        return import_path13.default.relative(baseInstallFolder, location);
      }));
      if (this.json) {
        this.context.stdout.write(`${JSON.stringify(outputPath)}
`);
      } else {
        this.context.stdout.write(`All done!
`);
      }
    }
  }
};

// sources/main.ts
function getPackageManagerRequestFromCli(parameter, engine) {
  if (!parameter)
    return null;
  const match = parameter.match(/^([^@]*)(?:@(.*))?$/);
  if (!match)
    return null;
  const [, binaryName, binaryVersion] = match;
  const packageManager = engine.getPackageManagerFor(binaryName);
  if (packageManager == null && binaryVersion == null) return null;
  return {
    packageManager,
    binaryName,
    binaryVersion: binaryVersion || null
  };
}
function isUsageError(error) {
  return error?.name === `UsageError`;
}
async function runMain(argv) {
  const engine = new Engine();
  const [firstArg, ...restArgs] = argv;
  const request = getPackageManagerRequestFromCli(firstArg, engine);
  if (!request) {
    const cli = new import_clipanion17.Cli({
      binaryLabel: `Corepack`,
      binaryName: `corepack`,
      binaryVersion: version
    });
    cli.register(import_clipanion17.Builtins.HelpCommand);
    cli.register(import_clipanion17.Builtins.VersionCommand);
    cli.register(CacheCommand);
    cli.register(DisableCommand);
    cli.register(EnableCommand);
    cli.register(InstallGlobalCommand);
    cli.register(InstallLocalCommand);
    cli.register(PackCommand);
    cli.register(UpCommand);
    cli.register(UseCommand);
    cli.register(HydrateCommand);
    cli.register(PrepareCommand);
    const context = {
      ...import_clipanion17.Cli.defaultContext,
      cwd: process.cwd(),
      engine
    };
    const code2 = await cli.run(argv, context);
    if (code2 !== 0) {
      process.exitCode ??= code2;
    }
  } else {
    try {
      await engine.executePackageManagerRequest(request, {
        cwd: process.cwd(),
        args: restArgs
      });
    } catch (error) {
      if (isUsageError(error)) {
        console.error(error.message);
        process.exit(1);
      }
      throw error;
    }
  }
}
// Annotate the CommonJS export names for ESM import in node:
0 && (module.exports = {
  runMain
});
/*! Bundled license information:

is-windows/index.js:
  (*!
   * is-windows <https://github.com/jonschlinkert/is-windows>
   *
   * Copyright © 2015-2018, Jon Schlinkert.
   * Released under the MIT License.
   *)
*/
