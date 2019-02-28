"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.merge = merge;
exports.validate = validate;
exports.normalizeReplacements = normalizeReplacements;

function _objectWithoutPropertiesLoose(source, excluded) { if (source == null) return {}; var target = {}; var sourceKeys = Object.keys(source); var key, i; for (i = 0; i < sourceKeys.length; i++) { key = sourceKeys[i]; if (excluded.indexOf(key) >= 0) continue; target[key] = source[key]; } return target; }

function merge(a, b) {
  const {
    placeholderWhitelist = a.placeholderWhitelist,
    placeholderPattern = a.placeholderPattern,
    preserveComments = a.preserveComments
  } = b;
  return {
    parser: Object.assign({}, a.parser, b.parser),
    placeholderWhitelist,
    placeholderPattern,
    preserveComments
  };
}

function validate(opts) {
  if (opts != null && typeof opts !== "object") {
    throw new Error("Unknown template options.");
  }

  const _ref = opts || {},
        {
    placeholderWhitelist,
    placeholderPattern,
    preserveComments
  } = _ref,
        parser = _objectWithoutPropertiesLoose(_ref, ["placeholderWhitelist", "placeholderPattern", "preserveComments"]);

  if (placeholderWhitelist != null && !(placeholderWhitelist instanceof Set)) {
    throw new Error("'.placeholderWhitelist' must be a Set, null, or undefined");
  }

  if (placeholderPattern != null && !(placeholderPattern instanceof RegExp) && placeholderPattern !== false) {
    throw new Error("'.placeholderPattern' must be a RegExp, false, null, or undefined");
  }

  if (preserveComments != null && typeof preserveComments !== "boolean") {
    throw new Error("'.preserveComments' must be a boolean, null, or undefined");
  }

  return {
    parser,
    placeholderWhitelist: placeholderWhitelist || undefined,
    placeholderPattern: placeholderPattern == null ? undefined : placeholderPattern,
    preserveComments: preserveComments == null ? false : preserveComments
  };
}

function normalizeReplacements(replacements) {
  if (Array.isArray(replacements)) {
    return replacements.reduce((acc, replacement, i) => {
      acc["$" + i] = replacement;
      return acc;
    }, {});
  } else if (typeof replacements === "object" || replacements == null) {
    return replacements || undefined;
  }

  throw new Error("Template replacements must be an array, object, null, or undefined");
}