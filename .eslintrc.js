'use strict';

const Module = require('module');
const path = require('path');

const NodePlugin = require('./tools/node_modules/eslint-plugin-node-core');
NodePlugin.RULES_DIR = path.resolve(__dirname, 'tools', 'eslint-rules');

const ModuleFindPath = Module._findPath;
const hacks = [
  'eslint-plugin-node-core',
  'eslint-plugin-markdown',
  'babel-eslint',
];
Module._findPath = (request, paths, isMain) => {
  const r = ModuleFindPath(request, paths, isMain);
  if (!r && hacks.includes(request)) {
    try {
      return require.resolve(`./tools/node_modules/${request}`);
    } catch (err) {
      return require.resolve(
        `./tools/node_modules/eslint/node_modules/${request}`);
    }
  }
  return r;
};

module.exports = {
  root: true,
  plugins: ['markdown', 'node-core'],
  env: { node: true, es6: true },
  parser: 'babel-eslint',
  parserOptions: { sourceType: 'script' },
  overrides: [
    {
      files: [
        'doc/api/esm.md',
        '*.mjs',
        'test/es-module/test-esm-example-loader.js',
      ],
      parserOptions: { sourceType: 'module' },
    },
  ],
  rules: {
    // ESLint built-in rules
    // http://eslint.org/docs/rules
    'accessor-pairs': 'error',
    'array-callback-return': 'error',
    'arrow-parens': ['error', 'always'],
    'arrow-spacing': ['error', { before: true, after: true }],
    'block-spacing': 'error',
    'brace-style': ['error', '1tbs', { allowSingleLine: true }],
    'comma-dangle': ['error', 'only-multiline'],
    'comma-spacing': 'error',
    'comma-style': 'error',
    'computed-property-spacing': 'error',
    'constructor-super': 'error',
    'dot-location': ['error', 'property'],
    'dot-notation': 'error',
    'eol-last': 'error',
    'eqeqeq': ['error', 'smart'],
    'for-direction': 'error',
    'func-call-spacing': 'error',
    'func-name-matching': 'error',
    'func-style': ['error', 'declaration', { allowArrowFunctions: true }],
    'indent': ['error', 2, {
      ArrayExpression: 'first',
      CallExpression: { arguments: 'first' },
      FunctionDeclaration: { parameters: 'first' },
      FunctionExpression: { parameters: 'first' },
      MemberExpression: 'off',
      ObjectExpression: 'first',
      SwitchCase: 1,
    }],
    'key-spacing': ['error', { mode: 'strict' }],
    'keyword-spacing': 'error',
    'linebreak-style': ['error', 'unix'],
    'max-len': ['error', {
      code: 80,
      ignorePattern: '^// Flags:',
      ignoreRegExpLiterals: true,
      ignoreUrls: true,
      tabWidth: 2,
    }],
    'new-parens': 'error',
    'no-class-assign': 'error',
    'no-confusing-arrow': 'error',
    'no-const-assign': 'error',
    'no-control-regex': 'error',
    'no-debugger': 'error',
    'no-delete-var': 'error',
    'no-dupe-args': 'error',
    'no-dupe-class-members': 'error',
    'no-dupe-keys': 'error',
    'no-duplicate-case': 'error',
    'no-empty-character-class': 'error',
    'no-ex-assign': 'error',
    'no-extra-boolean-cast': 'error',
    'no-extra-parens': ['error', 'functions'],
    'no-extra-semi': 'error',
    'no-fallthrough': 'error',
    'no-func-assign': 'error',
    'no-global-assign': 'error',
    'no-invalid-regexp': 'error',
    'no-irregular-whitespace': 'error',
    'no-lonely-if': 'error',
    'no-mixed-requires': 'error',
    'no-mixed-spaces-and-tabs': 'error',
    'no-multi-spaces': ['error', { ignoreEOLComments: true }],
    'no-multiple-empty-lines': ['error', { max: 2, maxEOF: 0, maxBOF: 0 }],
    'no-new-require': 'error',
    'no-new-symbol': 'error',
    'no-obj-calls': 'error',
    'no-octal': 'error',
    'no-path-concat': 'error',
    'no-proto': 'error',
    'no-redeclare': 'error',
    'no-restricted-modules': ['error', 'sys'],
    'no-restricted-properties': [
      'error',
      {
        object: 'assert',
        property: 'deepEqual',
        message: 'Use assert.deepStrictEqual().',
      },
      {
        object: 'assert',
        property: 'notDeepEqual',
        message: 'Use assert.notDeepStrictEqual().',
      },
      {
        object: 'assert',
        property: 'equal',
        message: 'Use assert.astrictEqual() rather than assert.equal().',
      },
      {
        object: 'assert',
        property: 'notEqual',
        message: 'Use assert.notStrictEqual() rather than assert.notEqual().',
      },
      {
        property: '__defineGetter__',
        message: '__defineGetter__ is deprecated.',
      },
      {
        property: '__defineSetter__',
        message: '__defineSetter__ is deprecated.',
      }
    ],
    /* eslint-disable max-len, quotes */
    // If this list is modified, please copy the change to lib/.eslintrc.yaml
    'no-restricted-syntax': [
      'error',
      {
        selector: "CallExpression[callee.object.name='assert'][callee.property.name='doesNotThrow']",
        message: "Please replace `assert.doesNotThrow()` and add a comment next to the code instead."
      },
      {
        selector: `CallExpression[callee.object.name='assert'][callee.property.name='throws'][arguments.1.type='Literal']:not([arguments.1.regex])`,
        message: 'use a regular expression for second argument of assert.throws()',
      },
      {
        selector: `CallExpression[callee.object.name='assert'][callee.property.name='throws'][arguments.length<2]`,
        message: 'assert.throws() must be invoked with at least two arguments.',
      },
      {
        selector: `CallExpression[callee.name='setTimeout'][arguments.length<2]`,
        message: 'setTimeout() must be invoked with at least two arguments.',
      },
      {
        selector: `CallExpression[callee.name='setInterval'][arguments.length<2]`,
        message: 'setInterval() must be invoked with at least 2 arguments.',
      },
      {
        selector: 'ThrowStatement > CallExpression[callee.name=/Error$/]',
        message: 'Use new keyword when throwing an Error.',
      }
    ],
    /* eslint-enable max-len, quotes */
    'no-return-await': 'error',
    'no-self-assign': 'error',
    'no-self-compare': 'error',
    'no-tabs': 'error',
    'no-template-curly-in-string': 'error',
    'no-this-before-super': 'error',
    'no-throw-literal': 'error',
    'no-trailing-spaces': 'error',
    'no-undef': 'error',
    'no-undef-init': 'error',
    'no-unexpected-multiline': 'error',
    'no-unreachable': 'error',
    'no-unsafe-finally': 'error',
    'no-unsafe-negation': 'error',
    'no-unused-labels': 'error',
    'no-unused-vars': ['error', { args: 'none' }],
    'no-use-before-define': ['error', {
      classes: true,
      functions: false,
      variables: false,
    }],
    'no-useless-call': 'error',
    'no-useless-concat': 'error',
    'no-useless-escape': 'error',
    'no-useless-return': 'error',
    'no-void': 'error',
    'no-whitespace-before-property': 'error',
    'no-with': 'error',
    'object-curly-spacing': ['error', 'always'],
    'one-var': ['error', { initialized: 'never' }],
    'one-var-declaration-per-line': 'error',
    'operator-linebreak': ['error', 'after'],
    'prefer-const': ['error', { ignoreReadBeforeAssign: true }],
    'quotes': ['error', 'single', { avoidEscape: true }],
    'quote-props': ['error', 'consistent'],
    'rest-spread-spacing': 'error',
    'semi': 'error',
    'semi-spacing': 'error',
    'space-before-blocks': ['error', 'always'],
    'space-before-function-paren': ['error', {
      anonymous: 'never',
      named: 'never',
      asyncArrow: 'always',
    }],
    'space-in-parens': ['error', 'never'],
    'space-infix-ops': 'error',
    'space-unary-ops': 'error',
    'spaced-comment': ['error', 'always', {
      'block': { 'balanced': true },
      'exceptions': ['-']
    }],
    'strict': ['error', 'global'],
    'symbol-description': 'error',
    'template-curly-spacing': 'error',
    'unicode-bom': 'error',
    'use-isnan': 'error',
    'valid-typeof': 'error',

    // Custom rules from eslint-plugin-node-core
    'node-core/no-unescaped-regexp-dot': 'error',
  },
  globals: {
    COUNTER_HTTP_CLIENT_REQUEST: false,
    COUNTER_HTTP_CLIENT_RESPONSE: false,
    COUNTER_HTTP_SERVER_REQUEST: false,
    COUNTER_HTTP_SERVER_RESPONSE: false,
    COUNTER_NET_SERVER_CONNECTION: false,
    COUNTER_NET_SERVER_CONNECTION_CLOSE: false,
    DTRACE_HTTP_CLIENT_REQUEST: false,
    DTRACE_HTTP_CLIENT_RESPONSE: false,
    DTRACE_HTTP_SERVER_REQUEST: false,
    DTRACE_HTTP_SERVER_RESPONSE: false,
    DTRACE_NET_SERVER_CONNECTION: false,
    DTRACE_NET_STREAM_END: false
  },
};
