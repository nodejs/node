"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = void 0;

var _escapeStringRegexp = _interopRequireDefault(require("escape-string-regexp"));

var _iterateJsdoc = _interopRequireDefault(require("../iterateJsdoc"));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

// https://babeljs.io/docs/en/babel-plugin-transform-react-jsx/
const jsxTagNames = new Set(['jsx', 'jsxFrag', 'jsxImportSource', 'jsxRuntime']);

var _default = (0, _iterateJsdoc.default)(({
  sourceCode,
  jsdoc,
  report,
  utils,
  context,
  settings,
  jsdocNode
}) => {
  const {
    definedTags = [],
    jsxTags
  } = context.options[0] || {};
  let definedPreferredTags = [];
  const {
    tagNamePreference,
    structuredTags
  } = settings;
  const definedStructuredTags = Object.keys(structuredTags);
  const definedNonPreferredTags = Object.keys(tagNamePreference);

  if (definedNonPreferredTags.length) {
    definedPreferredTags = Object.values(tagNamePreference).map(preferredTag => {
      if (typeof preferredTag === 'string') {
        // May become an empty string but will be filtered out below
        return preferredTag;
      }

      if (!preferredTag) {
        return undefined;
      }

      if (typeof preferredTag !== 'object') {
        utils.reportSettings('Invalid `settings.jsdoc.tagNamePreference`. Values must be falsy, a string, or an object.');
      }

      return preferredTag.replacement;
    }).filter(preferredType => {
      return preferredType;
    });
  }

  for (const jsdocTag of jsdoc.tags) {
    const tagName = jsdocTag.tag;

    if (jsxTags && jsxTagNames.has(tagName)) {
      continue;
    }

    if (utils.isValidTag(tagName, [...definedTags, ...definedPreferredTags, ...definedNonPreferredTags, ...definedStructuredTags])) {
      let preferredTagName = utils.getPreferredTagName({
        allowObjectReturn: true,
        defaultMessage: `Blacklisted tag found (\`@${tagName}\`)`,
        tagName
      });

      if (!preferredTagName) {
        continue;
      }

      let message;

      if (typeof preferredTagName === 'object') {
        ({
          message,
          replacement: preferredTagName
        } = preferredTagName);
      }

      if (!message) {
        message = `Invalid JSDoc tag (preference). Replace "${tagName}" JSDoc tag with "${preferredTagName}".`;
      }

      if (preferredTagName !== tagName) {
        report(message, fixer => {
          const replacement = sourceCode.getText(jsdocNode).replace(new RegExp(`@${(0, _escapeStringRegexp.default)(tagName)}\\b`, 'u'), `@${preferredTagName}`);
          return fixer.replaceText(jsdocNode, replacement);
        }, jsdocTag);
      }
    } else {
      report(`Invalid JSDoc tag name "${tagName}".`, null, jsdocTag);
    }
  }
}, {
  iterateAllJsdocs: true,
  meta: {
    docs: {
      description: 'Reports invalid block tag names.',
      url: 'https://github.com/gajus/eslint-plugin-jsdoc#eslint-plugin-jsdoc-rules-check-tag-names'
    },
    fixable: 'code',
    schema: [{
      additionalProperties: false,
      properties: {
        definedTags: {
          items: {
            type: 'string'
          },
          type: 'array'
        },
        jsxTags: {
          type: 'boolean'
        }
      },
      type: 'object'
    }],
    type: 'suggestion'
  }
});

exports.default = _default;
module.exports = exports.default;
//# sourceMappingURL=checkTagNames.js.map