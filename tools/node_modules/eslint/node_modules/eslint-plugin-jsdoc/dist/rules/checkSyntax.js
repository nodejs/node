"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = void 0;

var _iterateJsdoc = _interopRequireDefault(require("../iterateJsdoc"));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

var _default = (0, _iterateJsdoc.default)(({
  jsdoc,
  report,
  settings
}) => {
  const {
    mode
  } = settings; // Don't check for "permissive" and "closure"

  if (mode === 'jsdoc' || mode === 'typescript') {
    for (const tag of jsdoc.tags) {
      if (tag.type.slice(-1) === '=') {
        report('Syntax should not be Google Closure Compiler style.', null, tag);
        break;
      }
    }
  }
}, {
  iterateAllJsdocs: true,
  meta: {
    docs: {
      description: 'Reports against syntax not valid for the mode (e.g., Google Closure Compiler in non-Closure mode).',
      url: 'https://github.com/gajus/eslint-plugin-jsdoc#eslint-plugin-jsdoc-rules-check-syntax'
    },
    type: 'suggestion'
  }
});

exports.default = _default;
module.exports = exports.default;
//# sourceMappingURL=checkSyntax.js.map