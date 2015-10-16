'use strict';
const l10n = process.binding('l10n');
const util = require("util");

module.exports = function(key, fallback) {
  // currently this works like util.format, but the plan
  // is to evolve it into a template string tag.
  var args = Array.prototype.slice.call(arguments,2);
  return key + ': ' + util.format.apply(this, args);
};

Object.defineProperty(module.exports, 'locale', {
  value: l10n.locale
});
Object.defineProperty(module.exports, 'icu', {
  value: l10n.icu
});
