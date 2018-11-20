"use strict";

exports.__esModule = true;
exports.merge = merge;
exports.validate = validate;
exports.normalizeReplacements = normalizeReplacements;

function _objectWithoutProperties(source, excluded) { if (source == null) return {}; var target = {}; var sourceKeys = Object.keys(source); var key, i; for (i = 0; i < sourceKeys.length; i++) { key = sourceKeys[i]; if (excluded.indexOf(key) >= 0) continue; target[key] = source[key]; } if (Object.getOwnPropertySymbols) { var sourceSymbolKeys = Object.getOwnPropertySymbols(source); for (i = 0; i < sourceSymbolKeys.length; i++) { key = sourceSymbolKeys[i]; if (excluded.indexOf(key) >= 0) continue; if (!Object.prototype.propertyIsEnumerable.call(source, key)) continue; target[key] = source[key]; } } return target; }

function merge(a, b) {
  var _b$placeholderWhiteli = b.placeholderWhitelist,
      placeholderWhitelist = _b$placeholderWhiteli === void 0 ? a.placeholderWhitelist : _b$placeholderWhiteli,
      _b$placeholderPattern = b.placeholderPattern,
      placeholderPattern = _b$placeholderPattern === void 0 ? a.placeholderPattern : _b$placeholderPattern,
      _b$preserveComments = b.preserveComments,
      preserveComments = _b$preserveComments === void 0 ? a.preserveComments : _b$preserveComments;
  return {
    parser: Object.assign({}, a.parser, b.parser),
    placeholderWhitelist: placeholderWhitelist,
    placeholderPattern: placeholderPattern,
    preserveComments: preserveComments
  };
}

function validate(opts) {
  if (opts != null && typeof opts !== "object") {
    throw new Error("Unknown template options.");
  }

  var _ref = opts || {},
      placeholderWhitelist = _ref.placeholderWhitelist,
      placeholderPattern = _ref.placeholderPattern,
      preserveComments = _ref.preserveComments,
      parser = _objectWithoutProperties(_ref, ["placeholderWhitelist", "placeholderPattern", "preserveComments"]);

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
    parser: parser,
    placeholderWhitelist: placeholderWhitelist || undefined,
    placeholderPattern: placeholderPattern == null ? undefined : placeholderPattern,
    preserveComments: preserveComments == null ? false : preserveComments
  };
}

function normalizeReplacements(replacements) {
  if (Array.isArray(replacements)) {
    return replacements.reduce(function (acc, replacement, i) {
      acc["$" + i] = replacement;
      return acc;
    }, {});
  } else if (typeof replacements === "object" || replacements == null) {
    return replacements || undefined;
  }

  throw new Error("Template replacements must be an array, object, null, or undefined");
}