"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.ScopeFlag = exports.ClassElementType = exports.BindingFlag = void 0;
var ScopeFlag = {
  OTHER: 0,
  PROGRAM: 1,
  FUNCTION: 2,
  ARROW: 4,
  SIMPLE_CATCH: 8,
  SUPER: 16,
  DIRECT_SUPER: 32,
  CLASS: 64,
  STATIC_BLOCK: 128,
  TS_MODULE: 256,
  VAR: 387
};
exports.ScopeFlag = ScopeFlag;
var BindingFlag = {
  KIND_VALUE: 1,
  KIND_TYPE: 2,
  SCOPE_VAR: 4,
  SCOPE_LEXICAL: 8,
  SCOPE_FUNCTION: 16,
  SCOPE_OUTSIDE: 32,
  FLAG_NONE: 64,
  FLAG_CLASS: 128,
  FLAG_TS_ENUM: 256,
  FLAG_TS_CONST_ENUM: 512,
  FLAG_TS_EXPORT_ONLY: 1024,
  FLAG_FLOW_DECLARE_FN: 2048,
  FLAG_TS_IMPORT: 4096,
  FLAG_NO_LET_IN_LEXICAL: 8192,
  TYPE_CLASS: 8331,
  TYPE_LEXICAL: 8201,
  TYPE_CATCH_PARAM: 9,
  TYPE_VAR: 5,
  TYPE_FUNCTION: 17,
  TYPE_TS_INTERFACE: 130,
  TYPE_TS_TYPE: 2,
  TYPE_TS_ENUM: 8459,
  TYPE_TS_AMBIENT: 1024,
  TYPE_NONE: 64,
  TYPE_OUTSIDE: 65,
  TYPE_TS_CONST_ENUM: 8971,
  TYPE_TS_NAMESPACE: 1024,
  TYPE_TS_TYPE_IMPORT: 4098,
  TYPE_TS_VALUE_IMPORT: 4096,
  TYPE_FLOW_DECLARE_FN: 2048
};
exports.BindingFlag = BindingFlag;
var ClassElementType = {
  OTHER: 0,
  FLAG_STATIC: 4,
  KIND_GETTER: 2,
  KIND_SETTER: 1,
  KIND_ACCESSOR: 3,
  STATIC_GETTER: 6,
  STATIC_SETTER: 5,
  INSTANCE_GETTER: 2,
  INSTANCE_SETTER: 1
};
exports.ClassElementType = ClassElementType;

//# sourceMappingURL=scopeflags.js.map
