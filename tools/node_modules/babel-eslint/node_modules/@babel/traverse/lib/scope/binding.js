"use strict";

exports.__esModule = true;
exports.default = void 0;

var Binding = function () {
  function Binding(_ref) {
    var identifier = _ref.identifier,
        scope = _ref.scope,
        path = _ref.path,
        kind = _ref.kind;
    this.constantViolations = void 0;
    this.constant = void 0;
    this.referencePaths = void 0;
    this.referenced = void 0;
    this.references = void 0;
    this.hasDeoptedValue = void 0;
    this.hasValue = void 0;
    this.value = void 0;
    this.identifier = identifier;
    this.scope = scope;
    this.path = path;
    this.kind = kind;
    this.constantViolations = [];
    this.constant = true;
    this.referencePaths = [];
    this.referenced = false;
    this.references = 0;
    this.clearValue();
  }

  var _proto = Binding.prototype;

  _proto.deoptValue = function deoptValue() {
    this.clearValue();
    this.hasDeoptedValue = true;
  };

  _proto.setValue = function setValue(value) {
    if (this.hasDeoptedValue) return;
    this.hasValue = true;
    this.value = value;
  };

  _proto.clearValue = function clearValue() {
    this.hasDeoptedValue = false;
    this.hasValue = false;
    this.value = null;
  };

  _proto.reassign = function reassign(path) {
    this.constant = false;

    if (this.constantViolations.indexOf(path) !== -1) {
      return;
    }

    this.constantViolations.push(path);
  };

  _proto.reference = function reference(path) {
    if (this.referencePaths.indexOf(path) !== -1) {
      return;
    }

    this.referenced = true;
    this.references++;
    this.referencePaths.push(path);
  };

  _proto.dereference = function dereference() {
    this.references--;
    this.referenced = !!this.references;
  };

  return Binding;
}();

exports.default = Binding;