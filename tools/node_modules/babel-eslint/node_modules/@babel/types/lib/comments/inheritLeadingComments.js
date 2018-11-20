"use strict";

exports.__esModule = true;
exports.default = inheritLeadingComments;

var _inherit = _interopRequireDefault(require("../utils/inherit"));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function inheritLeadingComments(child, parent) {
  (0, _inherit.default)("leadingComments", child, parent);
}