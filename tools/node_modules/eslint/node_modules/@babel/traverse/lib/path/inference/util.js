"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.createUnionType = createUnionType;
var _t = require("@babel/types");
const {
  createFlowUnionType,
  createTSUnionType,
  createUnionTypeAnnotation,
  isFlowType,
  isTSType
} = _t;
function createUnionType(types) {
  {
    if (isFlowType(types[0])) {
      if (createFlowUnionType) {
        return createFlowUnionType(types);
      }
      return createUnionTypeAnnotation(types);
    } else {
      if (createTSUnionType) {
        return createTSUnionType(types);
      }
    }
  }
}

//# sourceMappingURL=util.js.map
