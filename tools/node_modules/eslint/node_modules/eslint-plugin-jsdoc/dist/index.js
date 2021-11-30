"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = void 0;

var _checkAccess = _interopRequireDefault(require("./rules/checkAccess"));

var _checkAlignment = _interopRequireDefault(require("./rules/checkAlignment"));

var _checkExamples = _interopRequireDefault(require("./rules/checkExamples"));

var _checkIndentation = _interopRequireDefault(require("./rules/checkIndentation"));

var _checkLineAlignment = _interopRequireDefault(require("./rules/checkLineAlignment"));

var _checkParamNames = _interopRequireDefault(require("./rules/checkParamNames"));

var _checkPropertyNames = _interopRequireDefault(require("./rules/checkPropertyNames"));

var _checkSyntax = _interopRequireDefault(require("./rules/checkSyntax"));

var _checkTagNames = _interopRequireDefault(require("./rules/checkTagNames"));

var _checkTypes = _interopRequireDefault(require("./rules/checkTypes"));

var _checkValues = _interopRequireDefault(require("./rules/checkValues"));

var _emptyTags = _interopRequireDefault(require("./rules/emptyTags"));

var _implementsOnClasses = _interopRequireDefault(require("./rules/implementsOnClasses"));

var _matchDescription = _interopRequireDefault(require("./rules/matchDescription"));

var _matchName = _interopRequireDefault(require("./rules/matchName"));

var _multilineBlocks = _interopRequireDefault(require("./rules/multilineBlocks"));

var _newlineAfterDescription = _interopRequireDefault(require("./rules/newlineAfterDescription"));

var _noBadBlocks = _interopRequireDefault(require("./rules/noBadBlocks"));

var _noDefaults = _interopRequireDefault(require("./rules/noDefaults"));

var _noMissingSyntax = _interopRequireDefault(require("./rules/noMissingSyntax"));

var _noMultiAsterisks = _interopRequireDefault(require("./rules/noMultiAsterisks"));

var _noRestrictedSyntax = _interopRequireDefault(require("./rules/noRestrictedSyntax"));

var _noTypes = _interopRequireDefault(require("./rules/noTypes"));

var _noUndefinedTypes = _interopRequireDefault(require("./rules/noUndefinedTypes"));

var _requireAsteriskPrefix = _interopRequireDefault(require("./rules/requireAsteriskPrefix"));

var _requireDescription = _interopRequireDefault(require("./rules/requireDescription"));

var _requireDescriptionCompleteSentence = _interopRequireDefault(require("./rules/requireDescriptionCompleteSentence"));

var _requireExample = _interopRequireDefault(require("./rules/requireExample"));

var _requireFileOverview = _interopRequireDefault(require("./rules/requireFileOverview"));

var _requireHyphenBeforeParamDescription = _interopRequireDefault(require("./rules/requireHyphenBeforeParamDescription"));

var _requireJsdoc = _interopRequireDefault(require("./rules/requireJsdoc"));

var _requireParam = _interopRequireDefault(require("./rules/requireParam"));

var _requireParamDescription = _interopRequireDefault(require("./rules/requireParamDescription"));

var _requireParamName = _interopRequireDefault(require("./rules/requireParamName"));

var _requireParamType = _interopRequireDefault(require("./rules/requireParamType"));

var _requireProperty = _interopRequireDefault(require("./rules/requireProperty"));

var _requirePropertyDescription = _interopRequireDefault(require("./rules/requirePropertyDescription"));

var _requirePropertyName = _interopRequireDefault(require("./rules/requirePropertyName"));

var _requirePropertyType = _interopRequireDefault(require("./rules/requirePropertyType"));

var _requireReturns = _interopRequireDefault(require("./rules/requireReturns"));

var _requireReturnsCheck = _interopRequireDefault(require("./rules/requireReturnsCheck"));

var _requireReturnsDescription = _interopRequireDefault(require("./rules/requireReturnsDescription"));

var _requireReturnsType = _interopRequireDefault(require("./rules/requireReturnsType"));

var _requireThrows = _interopRequireDefault(require("./rules/requireThrows"));

var _requireYields = _interopRequireDefault(require("./rules/requireYields"));

var _requireYieldsCheck = _interopRequireDefault(require("./rules/requireYieldsCheck"));

var _tagLines = _interopRequireDefault(require("./rules/tagLines"));

var _validTypes = _interopRequireDefault(require("./rules/validTypes"));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

var _default = {
  configs: {
    recommended: {
      plugins: ['jsdoc'],
      rules: {
        'jsdoc/check-access': 'warn',
        'jsdoc/check-alignment': 'warn',
        'jsdoc/check-examples': 'off',
        'jsdoc/check-indentation': 'off',
        'jsdoc/check-line-alignment': 'off',
        'jsdoc/check-param-names': 'warn',
        'jsdoc/check-property-names': 'warn',
        'jsdoc/check-syntax': 'off',
        'jsdoc/check-tag-names': 'warn',
        'jsdoc/check-types': 'warn',
        'jsdoc/check-values': 'warn',
        'jsdoc/empty-tags': 'warn',
        'jsdoc/implements-on-classes': 'warn',
        'jsdoc/match-description': 'off',
        'jsdoc/match-name': 'off',
        'jsdoc/multiline-blocks': 'warn',
        'jsdoc/newline-after-description': 'warn',
        'jsdoc/no-bad-blocks': 'off',
        'jsdoc/no-defaults': 'off',
        'jsdoc/no-missing-syntax': 'off',
        'jsdoc/no-multi-asterisks': 'warn',
        'jsdoc/no-restricted-syntax': 'off',
        'jsdoc/no-types': 'off',
        'jsdoc/no-undefined-types': 'warn',
        'jsdoc/require-asterisk-prefix': 'off',
        'jsdoc/require-description': 'off',
        'jsdoc/require-description-complete-sentence': 'off',
        'jsdoc/require-example': 'off',
        'jsdoc/require-file-overview': 'off',
        'jsdoc/require-hyphen-before-param-description': 'off',
        'jsdoc/require-jsdoc': 'warn',
        'jsdoc/require-param': 'warn',
        'jsdoc/require-param-description': 'warn',
        'jsdoc/require-param-name': 'warn',
        'jsdoc/require-param-type': 'warn',
        'jsdoc/require-property': 'warn',
        'jsdoc/require-property-description': 'warn',
        'jsdoc/require-property-name': 'warn',
        'jsdoc/require-property-type': 'warn',
        'jsdoc/require-returns': 'warn',
        'jsdoc/require-returns-check': 'warn',
        'jsdoc/require-returns-description': 'warn',
        'jsdoc/require-returns-type': 'warn',
        'jsdoc/require-throws': 'off',
        'jsdoc/require-yields': 'warn',
        'jsdoc/require-yields-check': 'warn',
        'jsdoc/tag-lines': 'warn',
        'jsdoc/valid-types': 'warn'
      }
    }
  },
  rules: {
    'check-access': _checkAccess.default,
    'check-alignment': _checkAlignment.default,
    'check-examples': _checkExamples.default,
    'check-indentation': _checkIndentation.default,
    'check-line-alignment': _checkLineAlignment.default,
    'check-param-names': _checkParamNames.default,
    'check-property-names': _checkPropertyNames.default,
    'check-syntax': _checkSyntax.default,
    'check-tag-names': _checkTagNames.default,
    'check-types': _checkTypes.default,
    'check-values': _checkValues.default,
    'empty-tags': _emptyTags.default,
    'implements-on-classes': _implementsOnClasses.default,
    'match-description': _matchDescription.default,
    'match-name': _matchName.default,
    'multiline-blocks': _multilineBlocks.default,
    'newline-after-description': _newlineAfterDescription.default,
    'no-bad-blocks': _noBadBlocks.default,
    'no-defaults': _noDefaults.default,
    'no-missing-syntax': _noMissingSyntax.default,
    'no-multi-asterisks': _noMultiAsterisks.default,
    'no-restricted-syntax': _noRestrictedSyntax.default,
    'no-types': _noTypes.default,
    'no-undefined-types': _noUndefinedTypes.default,
    'require-asterisk-prefix': _requireAsteriskPrefix.default,
    'require-description': _requireDescription.default,
    'require-description-complete-sentence': _requireDescriptionCompleteSentence.default,
    'require-example': _requireExample.default,
    'require-file-overview': _requireFileOverview.default,
    'require-hyphen-before-param-description': _requireHyphenBeforeParamDescription.default,
    'require-jsdoc': _requireJsdoc.default,
    'require-param': _requireParam.default,
    'require-param-description': _requireParamDescription.default,
    'require-param-name': _requireParamName.default,
    'require-param-type': _requireParamType.default,
    'require-property': _requireProperty.default,
    'require-property-description': _requirePropertyDescription.default,
    'require-property-name': _requirePropertyName.default,
    'require-property-type': _requirePropertyType.default,
    'require-returns': _requireReturns.default,
    'require-returns-check': _requireReturnsCheck.default,
    'require-returns-description': _requireReturnsDescription.default,
    'require-returns-type': _requireReturnsType.default,
    'require-throws': _requireThrows.default,
    'require-yields': _requireYields.default,
    'require-yields-check': _requireYieldsCheck.default,
    'tag-lines': _tagLines.default,
    'valid-types': _validTypes.default
  }
};
exports.default = _default;
module.exports = exports.default;
//# sourceMappingURL=index.js.map