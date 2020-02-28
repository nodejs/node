'use strict';

/* eslint-env node */

const Module = require('module');
const path = require('path');

const NodePlugin = require('./tools/node_modules/eslint-plugin-node-core');
NodePlugin.RULES_DIR = path.resolve(__dirname, 'tools', 'eslint-rules');

// The Module._findPath() monkeypatching is to make it so that ESLint will work
// if invoked by a globally-installed ESLint or ESLint installed elsewhere
// rather than the one we ship. This makes it possible for IDEs to lint files
// with our rules while people edit them.
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
    // Keep the variable in place to ensure that ESLint started by older Node.js
    // versions work as expected.
    // eslint-disable-next-line no-unused-vars
    } catch (e) {
      return require.resolve(
        `./tools/node_modules/eslint/node_modules/${request}`);
    }
  }
  return r;
};

module.exports = {
  root: true,
  plugins: ['markdown', 'node-core'],
  parser: 'babel-eslint',
  parserOptions: { sourceType: 'script' },
  overrides: [
    {
      files: [
        'doc/api/esm.md',
        'doc/api/modules.md',
        'test/es-module/test-esm-type-flag.js',
        'test/es-module/test-esm-type-flag-alias.js',
        '*.mjs',
        'test/es-module/test-esm-example-loader.js',
      ],
      parserOptions: { sourceType: 'module' },
    },
    {
      files: ['**/*.md'],
      parserOptions: { ecmaFeatures: { impliedStrict: true } },
      rules: { strict: 'off' },
    },
  ],
  rules: {
    // ESLint built-in rules
    // http://eslint.org/docs/rules
    'accessor-pairs': 'error',
    'array-callback-return': 'error',
    'arrow-parens': ['error', 'always'],
    'arrow-spacing': ['error', { before: true, after: true }],
    'block-scoped-var': 'error',
    'block-spacing': 'error',
    'brace-style': ['error', '1tbs', { allowSingleLine: true }],
    'capitalized-comments': ['error', 'always', {
      line: {
        // Ignore all lines that have less characters than 20 and all lines that
        // start with something that looks like a variable name or code.
        // eslint-disable-next-line max-len
        ignorePattern: '.{0,20}$|[a-z]+ ?[0-9A-Z_.(/=:[#-]|std|http|ssh|ftp|(let|var|const) [a-z_A-Z0-9]+ =|[b-z] |[a-z]*[0-9].* ',
        ignoreInlineComments: true,
        ignoreConsecutiveComments: true,
      },
      block: {
        ignorePattern: '.*',
      },
    }],
    'comma-dangle': ['error', 'only-multiline'],
    'comma-spacing': 'error',
    'comma-style': 'error',
    'computed-property-spacing': 'error',
    'constructor-super': 'error',
    'default-case-last': 'error',
    'dot-location': ['error', 'property'],
    'dot-notation': 'error',
    'eol-last': 'error',
    'eqeqeq': ['error', 'smart'],
    'for-direction': 'error',
    'func-call-spacing': 'error',
    'func-name-matching': 'error',
    'func-style': ['error', 'declaration', { allowArrowFunctions: true }],
    'getter-return': 'error',
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
    'no-async-promise-executor': 'error',
    'no-class-assign': 'error',
    'no-confusing-arrow': 'error',
    'no-const-assign': 'error',
    'no-constructor-return': 'error',
    'no-control-regex': 'error',
    'no-debugger': 'error',
    'no-delete-var': 'error',
    'no-dupe-args': 'error',
    'no-dupe-class-members': 'error',
    'no-dupe-keys': 'error',
    'no-dupe-else-if': 'error',
    'no-duplicate-case': 'error',
    'no-duplicate-imports': 'error',
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
    'no-misleading-character-class': 'error',
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
    'no-redeclare': ['error', { 'builtinGlobals': false }],
    'no-restricted-modules': ['error', 'sys'],
    /* eslint-disable max-len */
    'no-restricted-properties': [
      'error',
      {
        object: 'assert',
        property: 'deepEqual',
        message: 'Use `assert.deepStrictEqual()`.',
      },
      {
        object: 'assert',
        property: 'notDeepEqual',
        message: 'Use `assert.notDeepStrictEqual()`.',
      },
      {
        object: 'assert',
        property: 'equal',
        message: 'Use `assert.strictEqual()` rather than `assert.equal()`.',
      },
      {
        object: 'assert',
        property: 'notEqual',
        message: 'Use `assert.notStrictEqual()` rather than `assert.notEqual()`.',
      },
      {
        property: '__defineGetter__',
        message: '__defineGetter__ is deprecated.',
      },
      {
        property: '__defineSetter__',
        message: '__defineSetter__ is deprecated.',
      },
    ],
    // If this list is modified, please copy changes that should apply to ./lib
    // as well to lib/.eslintrc.yaml.
    'no-restricted-syntax': [
      'error',
      {
        selector: "CallExpression[callee.property.name='deepStrictEqual'][arguments.2.type='Literal']",
        message: 'Do not use a literal for the third argument of assert.deepStrictEqual()',
      },
      {
        selector: "CallExpression[callee.property.name='doesNotThrow']",
        message: 'Do not use `assert.doesNotThrow()`. Write the code without the wrapper and add a comment instead.',
      },
      {
        selector: "CallExpression[callee.property.name='doesNotReject']",
        message: 'Do not use `assert.doesNotReject()`. Write the code without the wrapper and add a comment instead.',
      },
      {
        selector: "CallExpression[callee.property.name='rejects'][arguments.length<2]",
        message: '`assert.rejects()` must be invoked with at least two arguments.',
      },
      {
        selector: "CallExpression[callee.property.name='strictEqual'][arguments.2.type='Literal']",
        message: 'Do not use a literal for the third argument of assert.strictEqual()',
      },
      {
        selector: "CallExpression[callee.property.name='throws'][arguments.1.type='Literal']:not([arguments.1.regex])",
        message: 'Use an object as second argument of `assert.throws()`.',
      },
      {
        selector: "CallExpression[callee.property.name='throws'][arguments.length<2]",
        message: '`assert.throws()` must be invoked with at least two arguments.',
      },
      {
        selector: "CallExpression[callee.name='setTimeout'][arguments.length<2]",
        message: '`setTimeout()` must be invoked with at least two arguments.',
      },
      {
        selector: "CallExpression[callee.name='setInterval'][arguments.length<2]",
        message: '`setInterval()` must be invoked with at least two arguments.',
      },
      {
        selector: 'ThrowStatement > CallExpression[callee.name=/Error$/]',
        message: 'Use `new` keyword when throwing an `Error`.',
      },
      {
        selector: "CallExpression[callee.property.name='notDeepStrictEqual'][arguments.0.type='Literal']:not([arguments.1.type='Literal']):not([arguments.1.type='ObjectExpression']):not([arguments.1.type='ArrayExpression']):not([arguments.1.type='UnaryExpression'])",
        message: 'The first argument should be the `actual`, not the `expected` value.',
      },
      {
        selector: "CallExpression[callee.property.name='notStrictEqual'][arguments.0.type='Literal']:not([arguments.1.type='Literal']):not([arguments.1.type='ObjectExpression']):not([arguments.1.type='ArrayExpression']):not([arguments.1.type='UnaryExpression'])",
        message: 'The first argument should be the `actual`, not the `expected` value.',
      },
      {
        selector: "CallExpression[callee.property.name='deepStrictEqual'][arguments.0.type='Literal']:not([arguments.1.type='Literal']):not([arguments.1.type='ObjectExpression']):not([arguments.1.type='ArrayExpression']):not([arguments.1.type='UnaryExpression'])",
        message: 'The first argument should be the `actual`, not the `expected` value.',
      },
      {
        selector: "CallExpression[callee.property.name='strictEqual'][arguments.0.type='Literal']:not([arguments.1.type='Literal']):not([arguments.1.type='ObjectExpression']):not([arguments.1.type='ArrayExpression']):not([arguments.1.type='UnaryExpression'])",
        message: 'The first argument should be the `actual`, not the `expected` value.',
      },
      {
        selector: "CallExpression[callee.name='isNaN']",
        message: 'Use Number.isNaN() instead of the global isNaN() function.',
      },
    ],
    /* eslint-enable max-len */
    'no-return-await': 'error',
    'no-self-assign': 'error',
    'no-self-compare': 'error',
    'no-setter-return': 'error',
    'no-shadow-restricted-names': 'error',
    'no-tabs': 'error',
    'no-template-curly-in-string': 'error',
    'no-this-before-super': 'error',
    'no-throw-literal': 'error',
    'no-trailing-spaces': 'error',
    'no-undef': ['error', { typeof: true }],
    'no-undef-init': 'error',
    'no-unexpected-multiline': 'error',
    'no-unreachable': 'error',
    'no-unsafe-finally': 'error',
    'no-unsafe-negation': 'error',
    'no-unused-labels': 'error',
    'no-unused-vars': ['error', { args: 'none', caughtErrors: 'all' }],
    'no-use-before-define': ['error', {
      classes: true,
      functions: false,
      variables: false,
    }],
    'no-useless-backreference': 'error',
    'no-useless-call': 'error',
    'no-useless-catch': 'error',
    'no-useless-concat': 'error',
    'no-useless-constructor': 'error',
    'no-useless-escape': 'error',
    'no-useless-return': 'error',
    'no-void': 'error',
    'no-whitespace-before-property': 'error',
    'no-with': 'error',
    'object-curly-spacing': ['error', 'always'],
    'one-var': ['error', { initialized: 'never' }],
    'one-var-declaration-per-line': 'error',
    'operator-linebreak': ['error', 'after'],
    'padding-line-between-statements': [
      'error',
      { blankLine: 'always', prev: 'function', next: 'function' },
    ],
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
      'exceptions': ['-'],
    }],
    'strict': ['error', 'global'],
    'symbol-description': 'error',
    'template-curly-spacing': 'error',
    'unicode-bom': 'error',
    'use-isnan': 'error',
    'valid-typeof': 'error',

    // Custom rules from eslint-plugin-node-core
    'node-core/no-unescaped-regexp-dot': 'error',
    'node-core/no-duplicate-requires': 'error',
  },
  globals: {
    Atomics: 'readable',
    BigInt: 'readable',
    BigInt64Array: 'readable',
    BigUint64Array: 'readable',
    TextEncoder: 'readable',
    TextDecoder: 'readable',
    queueMicrotask: 'readable',
    globalThis: 'readable',
  },
};
