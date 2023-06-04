"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.instantiate = exports.ParseErrorCode = void 0;
var ParseErrorCode = {
  SyntaxError: "BABEL_PARSER_SYNTAX_ERROR",
  SourceTypeModuleError: "BABEL_PARSER_SOURCETYPE_MODULE_REQUIRED"
};
exports.ParseErrorCode = ParseErrorCode;
const reflect = (keys, last = keys.length - 1) => ({
  get() {
    return keys.reduce((object, key) => object[key], this);
  },
  set(value) {
    keys.reduce((item, key, i) => i === last ? item[key] = value : item[key], this);
  }
});
const instantiate = (constructor, properties, descriptors) => Object.keys(descriptors).map(key => [key, descriptors[key]]).filter(([, descriptor]) => !!descriptor).map(([key, descriptor]) => [key, typeof descriptor === "function" ? {
  value: descriptor,
  enumerable: false
} : typeof descriptor.reflect === "string" ? Object.assign({}, descriptor, reflect(descriptor.reflect.split("."))) : descriptor]).reduce((instance, [key, descriptor]) => Object.defineProperty(instance, key, Object.assign({
  configurable: true
}, descriptor)), Object.assign(new constructor(), properties));
exports.instantiate = instantiate;

//# sourceMappingURL=credentials.js.map
