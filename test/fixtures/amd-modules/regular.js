var R = require;
var E = exports;
var M = module;

define(function(require, exports, module, nothingHere) {
  if (R !== require) throw new Error("invalid require in AMD cb");
  if (E !== exports) throw new Error("invalid exports in AMD cb");
  if (M !== module) throw new Error("invalid module in AMD cb");
  if (nothingHere !== undefined) {
    throw new Error("unknown args to AMD cb");
  }

  exports.ok = { ok: true };
});
