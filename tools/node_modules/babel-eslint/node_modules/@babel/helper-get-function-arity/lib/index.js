"use strict";

exports.__esModule = true;
exports.default = _default;

var t = _interopRequireWildcard(require("@babel/types"));

function _interopRequireWildcard(obj) { if (obj && obj.__esModule) { return obj; } else { var newObj = {}; if (obj != null) { for (var key in obj) { if (Object.prototype.hasOwnProperty.call(obj, key)) { var desc = Object.defineProperty && Object.getOwnPropertyDescriptor ? Object.getOwnPropertyDescriptor(obj, key) : {}; if (desc.get || desc.set) { Object.defineProperty(newObj, key, desc); } else { newObj[key] = obj[key]; } } } } newObj.default = obj; return newObj; } }

function _default(node) {
  var params = node.params;

  for (var i = 0; i < params.length; i++) {
    var param = params[i];

    if (t.isAssignmentPattern(param) || t.isRestElement(param)) {
      return i;
    }
  }

  return params.length;
}