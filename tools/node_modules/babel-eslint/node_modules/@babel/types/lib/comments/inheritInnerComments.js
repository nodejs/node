"use strict";

exports.__esModule = true;
exports.default = inheritInnerComments;

var _inherit = _interopRequireDefault(require("../utils/inherit"));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function inheritInnerComments(child, parent) {
  (0, _inherit.default)("innerComments", child, parent);
}