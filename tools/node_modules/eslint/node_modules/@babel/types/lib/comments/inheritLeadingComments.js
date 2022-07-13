"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = inheritLeadingComments;

var _inherit = require("../utils/inherit");

function inheritLeadingComments(child, parent) {
  (0, _inherit.default)("leadingComments", child, parent);
}