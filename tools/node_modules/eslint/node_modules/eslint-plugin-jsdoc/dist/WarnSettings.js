"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = void 0;

const WarnSettings = function () {
  /** @type {WeakMap<object, Set<string>>} */
  const warnedSettings = new WeakMap();
  return {
    /**
     * Warn only once for each context and setting
     *
     * @param {object} context
     * @param {string} setting
     */
    hasBeenWarned(context, setting) {
      return warnedSettings.has(context) && warnedSettings.get(context).has(setting);
    },

    markSettingAsWarned(context, setting) {
      // istanbul ignore else
      if (!warnedSettings.has(context)) {
        warnedSettings.set(context, new Set());
      }

      warnedSettings.get(context).add(setting);
    }

  };
};

var _default = WarnSettings;
exports.default = _default;
module.exports = exports.default;
//# sourceMappingURL=WarnSettings.js.map