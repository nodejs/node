"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = void 0;
var _checkAccess = _interopRequireDefault(require("./rules/checkAccess.js"));
var _checkAlignment = _interopRequireDefault(require("./rules/checkAlignment.js"));
var _checkExamples = _interopRequireDefault(require("./rules/checkExamples.js"));
var _checkIndentation = _interopRequireDefault(require("./rules/checkIndentation.js"));
var _checkLineAlignment = _interopRequireDefault(require("./rules/checkLineAlignment.js"));
var _checkParamNames = _interopRequireDefault(require("./rules/checkParamNames.js"));
var _checkPropertyNames = _interopRequireDefault(require("./rules/checkPropertyNames.js"));
var _checkSyntax = _interopRequireDefault(require("./rules/checkSyntax.js"));
var _checkTagNames = _interopRequireDefault(require("./rules/checkTagNames.js"));
var _checkTypes = _interopRequireDefault(require("./rules/checkTypes.js"));
var _checkValues = _interopRequireDefault(require("./rules/checkValues.js"));
var _emptyTags = _interopRequireDefault(require("./rules/emptyTags.js"));
var _implementsOnClasses = _interopRequireDefault(require("./rules/implementsOnClasses.js"));
var _importsAsDependencies = _interopRequireDefault(require("./rules/importsAsDependencies.js"));
var _informativeDocs = _interopRequireDefault(require("./rules/informativeDocs.js"));
var _matchDescription = _interopRequireDefault(require("./rules/matchDescription.js"));
var _matchName = _interopRequireDefault(require("./rules/matchName.js"));
var _multilineBlocks = _interopRequireDefault(require("./rules/multilineBlocks.js"));
var _noBadBlocks = _interopRequireDefault(require("./rules/noBadBlocks.js"));
var _noBlankBlockDescriptions = _interopRequireDefault(require("./rules/noBlankBlockDescriptions.js"));
var _noBlankBlocks = _interopRequireDefault(require("./rules/noBlankBlocks.js"));
var _noDefaults = _interopRequireDefault(require("./rules/noDefaults.js"));
var _noMissingSyntax = _interopRequireDefault(require("./rules/noMissingSyntax.js"));
var _noMultiAsterisks = _interopRequireDefault(require("./rules/noMultiAsterisks.js"));
var _noRestrictedSyntax = _interopRequireDefault(require("./rules/noRestrictedSyntax.js"));
var _noTypes = _interopRequireDefault(require("./rules/noTypes.js"));
var _noUndefinedTypes = _interopRequireDefault(require("./rules/noUndefinedTypes.js"));
var _requireAsteriskPrefix = _interopRequireDefault(require("./rules/requireAsteriskPrefix.js"));
var _requireDescription = _interopRequireDefault(require("./rules/requireDescription.js"));
var _requireDescriptionCompleteSentence = _interopRequireDefault(require("./rules/requireDescriptionCompleteSentence.js"));
var _requireExample = _interopRequireDefault(require("./rules/requireExample.js"));
var _requireFileOverview = _interopRequireDefault(require("./rules/requireFileOverview.js"));
var _requireHyphenBeforeParamDescription = _interopRequireDefault(require("./rules/requireHyphenBeforeParamDescription.js"));
var _requireJsdoc = _interopRequireDefault(require("./rules/requireJsdoc.js"));
var _requireParam = _interopRequireDefault(require("./rules/requireParam.js"));
var _requireParamDescription = _interopRequireDefault(require("./rules/requireParamDescription.js"));
var _requireParamName = _interopRequireDefault(require("./rules/requireParamName.js"));
var _requireParamType = _interopRequireDefault(require("./rules/requireParamType.js"));
var _requireProperty = _interopRequireDefault(require("./rules/requireProperty.js"));
var _requirePropertyDescription = _interopRequireDefault(require("./rules/requirePropertyDescription.js"));
var _requirePropertyName = _interopRequireDefault(require("./rules/requirePropertyName.js"));
var _requirePropertyType = _interopRequireDefault(require("./rules/requirePropertyType.js"));
var _requireReturns = _interopRequireDefault(require("./rules/requireReturns.js"));
var _requireReturnsCheck = _interopRequireDefault(require("./rules/requireReturnsCheck.js"));
var _requireReturnsDescription = _interopRequireDefault(require("./rules/requireReturnsDescription.js"));
var _requireReturnsType = _interopRequireDefault(require("./rules/requireReturnsType.js"));
var _requireThrows = _interopRequireDefault(require("./rules/requireThrows.js"));
var _requireYields = _interopRequireDefault(require("./rules/requireYields.js"));
var _requireYieldsCheck = _interopRequireDefault(require("./rules/requireYieldsCheck.js"));
var _sortTags = _interopRequireDefault(require("./rules/sortTags.js"));
var _tagLines = _interopRequireDefault(require("./rules/tagLines.js"));
var _textEscaping = _interopRequireDefault(require("./rules/textEscaping.js"));
var _validTypes = _interopRequireDefault(require("./rules/validTypes.js"));
function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }
/**
 * @type {import('eslint').ESLint.Plugin & {
 *   configs: Record<string, import('eslint').ESLint.ConfigData|{}>
 * }}
 */
const index = {
  configs: {},
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
    'imports-as-dependencies': _importsAsDependencies.default,
    'informative-docs': _informativeDocs.default,
    'match-description': _matchDescription.default,
    'match-name': _matchName.default,
    'multiline-blocks': _multilineBlocks.default,
    'no-bad-blocks': _noBadBlocks.default,
    'no-blank-block-descriptions': _noBlankBlockDescriptions.default,
    'no-blank-blocks': _noBlankBlocks.default,
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
    'sort-tags': _sortTags.default,
    'tag-lines': _tagLines.default,
    'text-escaping': _textEscaping.default,
    'valid-types': _validTypes.default
  }
};

/**
 * @param {"warn"|"error"} warnOrError
 * @param {boolean} [flat]
 * @returns {import('eslint').ESLint.ConfigData | {plugins: {}, rules: {}}}
 */
const createRecommendedRuleset = (warnOrError, flat) => {
  return {
    plugins: flat ? {
      jsdoc: index
    } : ['jsdoc'],
    rules: {
      'jsdoc/check-access': warnOrError,
      'jsdoc/check-alignment': warnOrError,
      'jsdoc/check-examples': 'off',
      'jsdoc/check-indentation': 'off',
      'jsdoc/check-line-alignment': 'off',
      'jsdoc/check-param-names': warnOrError,
      'jsdoc/check-property-names': warnOrError,
      'jsdoc/check-syntax': 'off',
      'jsdoc/check-tag-names': warnOrError,
      'jsdoc/check-types': warnOrError,
      'jsdoc/check-values': warnOrError,
      'jsdoc/empty-tags': warnOrError,
      'jsdoc/implements-on-classes': warnOrError,
      'jsdoc/imports-as-dependencies': 'off',
      'jsdoc/informative-docs': 'off',
      'jsdoc/match-description': 'off',
      'jsdoc/match-name': 'off',
      'jsdoc/multiline-blocks': warnOrError,
      'jsdoc/no-bad-blocks': 'off',
      'jsdoc/no-blank-block-descriptions': 'off',
      'jsdoc/no-blank-blocks': 'off',
      'jsdoc/no-defaults': warnOrError,
      'jsdoc/no-missing-syntax': 'off',
      'jsdoc/no-multi-asterisks': warnOrError,
      'jsdoc/no-restricted-syntax': 'off',
      'jsdoc/no-types': 'off',
      'jsdoc/no-undefined-types': warnOrError,
      'jsdoc/require-asterisk-prefix': 'off',
      'jsdoc/require-description': 'off',
      'jsdoc/require-description-complete-sentence': 'off',
      'jsdoc/require-example': 'off',
      'jsdoc/require-file-overview': 'off',
      'jsdoc/require-hyphen-before-param-description': 'off',
      'jsdoc/require-jsdoc': warnOrError,
      'jsdoc/require-param': warnOrError,
      'jsdoc/require-param-description': warnOrError,
      'jsdoc/require-param-name': warnOrError,
      'jsdoc/require-param-type': warnOrError,
      'jsdoc/require-property': warnOrError,
      'jsdoc/require-property-description': warnOrError,
      'jsdoc/require-property-name': warnOrError,
      'jsdoc/require-property-type': warnOrError,
      'jsdoc/require-returns': warnOrError,
      'jsdoc/require-returns-check': warnOrError,
      'jsdoc/require-returns-description': warnOrError,
      'jsdoc/require-returns-type': warnOrError,
      'jsdoc/require-throws': 'off',
      'jsdoc/require-yields': warnOrError,
      'jsdoc/require-yields-check': warnOrError,
      'jsdoc/sort-tags': 'off',
      'jsdoc/tag-lines': warnOrError,
      'jsdoc/text-escaping': 'off',
      'jsdoc/valid-types': warnOrError
    }
  };
};

/**
 * @param {"warn"|"error"} warnOrError
 * @param {boolean} [flat]
 * @returns {import('eslint').ESLint.ConfigData|{}}
 */
const createRecommendedTypeScriptRuleset = (warnOrError, flat) => {
  const ruleset = createRecommendedRuleset(warnOrError, flat);
  return {
    ...ruleset,
    rules: {
      ...ruleset.rules,
      /* eslint-disable indent -- Extra indent to avoid use by auto-rule-editing */
      'jsdoc/check-tag-names': [warnOrError, {
        typed: true
      }],
      'jsdoc/no-types': warnOrError,
      'jsdoc/no-undefined-types': 'off',
      'jsdoc/require-param-type': 'off',
      'jsdoc/require-property-type': 'off',
      'jsdoc/require-returns-type': 'off'
      /* eslint-enable indent */
    }
  };
};

/**
 * @param {"warn"|"error"} warnOrError
 * @param {boolean} [flat]
 * @returns {import('eslint').ESLint.ConfigData|{}}
 */
const createRecommendedTypeScriptFlavorRuleset = (warnOrError, flat) => {
  const ruleset = createRecommendedRuleset(warnOrError, flat);
  return {
    ...ruleset,
    rules: {
      ...ruleset.rules,
      /* eslint-disable indent -- Extra indent to avoid use by auto-rule-editing */
      'jsdoc/no-undefined-types': 'off'
      /* eslint-enable indent */
    }
  };
};

/* istanbul ignore if -- TS */
if (!index.configs) {
  throw new Error('TypeScript guard');
}
index.configs.recommended = createRecommendedRuleset('warn');
index.configs['recommended-error'] = createRecommendedRuleset('error');
index.configs['recommended-typescript'] = createRecommendedTypeScriptRuleset('warn');
index.configs['recommended-typescript-error'] = createRecommendedTypeScriptRuleset('error');
index.configs['recommended-typescript-flavor'] = createRecommendedTypeScriptFlavorRuleset('warn');
index.configs['recommended-typescript-flavor-error'] = createRecommendedTypeScriptFlavorRuleset('error');
index.configs['flat/recommended'] = createRecommendedRuleset('warn', true);
index.configs['flat/recommended-error'] = createRecommendedRuleset('error', true);
index.configs['flat/recommended-typescript'] = createRecommendedTypeScriptRuleset('warn', true);
index.configs['flat/recommended-typescript-error'] = createRecommendedTypeScriptRuleset('error', true);
index.configs['flat/recommended-typescript-flavor'] = createRecommendedTypeScriptFlavorRuleset('warn', true);
index.configs['flat/recommended-typescript-flavor-error'] = createRecommendedTypeScriptFlavorRuleset('error', true);
var _default = index;
exports.default = _default;
module.exports = exports.default;
//# sourceMappingURL=index.js.map