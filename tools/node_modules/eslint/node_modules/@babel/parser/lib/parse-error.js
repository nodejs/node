"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
var _exportNames = {
  ParseErrorEnum: true,
  Errors: true
};
exports.Errors = void 0;
exports.ParseErrorEnum = ParseErrorEnum;
var _location = require("./util/location");
var _credentials = require("./parse-error/credentials");
Object.keys(_credentials).forEach(function (key) {
  if (key === "default" || key === "__esModule") return;
  if (Object.prototype.hasOwnProperty.call(_exportNames, key)) return;
  if (key in exports && exports[key] === _credentials[key]) return;
  Object.defineProperty(exports, key, {
    enumerable: true,
    get: function () {
      return _credentials[key];
    }
  });
});
var _moduleErrors = require("./parse-error/module-errors");
var _standardErrors = require("./parse-error/standard-errors");
var _strictModeErrors = require("./parse-error/strict-mode-errors");
var _pipelineOperatorErrors = require("./parse-error/pipeline-operator-errors");
const _excluded = ["toMessage"],
  _excluded2 = ["message"];
function _objectWithoutPropertiesLoose(source, excluded) { if (source == null) return {}; var target = {}; var sourceKeys = Object.keys(source); var key, i; for (i = 0; i < sourceKeys.length; i++) { key = sourceKeys[i]; if (excluded.indexOf(key) >= 0) continue; target[key] = source[key]; } return target; }
function toParseErrorConstructor(_ref) {
  let {
      toMessage
    } = _ref,
    properties = _objectWithoutPropertiesLoose(_ref, _excluded);
  return function constructor({
    loc,
    details
  }) {
    return (0, _credentials.instantiate)(SyntaxError, Object.assign({}, properties, {
      loc
    }), {
      clone(overrides = {}) {
        const loc = overrides.loc || {};
        return constructor({
          loc: new _location.Position("line" in loc ? loc.line : this.loc.line, "column" in loc ? loc.column : this.loc.column, "index" in loc ? loc.index : this.loc.index),
          details: Object.assign({}, this.details, overrides.details)
        });
      },
      details: {
        value: details,
        enumerable: false
      },
      message: {
        get() {
          return `${toMessage(this.details)} (${this.loc.line}:${this.loc.column})`;
        },
        set(value) {
          Object.defineProperty(this, "message", {
            value
          });
        }
      },
      pos: {
        reflect: "loc.index",
        enumerable: true
      },
      missingPlugin: "missingPlugin" in details && {
        reflect: "details.missingPlugin",
        enumerable: true
      }
    });
  };
}
function ParseErrorEnum(argument, syntaxPlugin) {
  if (Array.isArray(argument)) {
    return parseErrorTemplates => ParseErrorEnum(parseErrorTemplates, argument[0]);
  }
  const ParseErrorConstructors = {};
  for (const reasonCode of Object.keys(argument)) {
    const template = argument[reasonCode];
    const _ref2 = typeof template === "string" ? {
        message: () => template
      } : typeof template === "function" ? {
        message: template
      } : template,
      {
        message
      } = _ref2,
      rest = _objectWithoutPropertiesLoose(_ref2, _excluded2);
    const toMessage = typeof message === "string" ? () => message : message;
    ParseErrorConstructors[reasonCode] = toParseErrorConstructor(Object.assign({
      code: _credentials.ParseErrorCode.SyntaxError,
      reasonCode,
      toMessage
    }, syntaxPlugin ? {
      syntaxPlugin
    } : {}, rest));
  }
  return ParseErrorConstructors;
}
const Errors = Object.assign({}, ParseErrorEnum(_moduleErrors.default), ParseErrorEnum(_standardErrors.default), ParseErrorEnum(_strictModeErrors.default), ParseErrorEnum`pipelineOperator`(_pipelineOperatorErrors.default));
exports.Errors = Errors;

//# sourceMappingURL=parse-error.js.map
