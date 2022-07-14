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
  'eslint-plugin-jsdoc',
  'eslint-plugin-markdown',
  '@babel/eslint-parser',
  '@babel/plugin-syntax-import-assertions',
];
Module._findPath = (request, paths, isMain) => {
  const r = ModuleFindPath(request, paths, isMain);
  if (!r && hacks.includes(request)) {
    try {
      return require.resolve(`./tools/node_modules/${request}`);
    } catch {
      return require.resolve(
        `./tools/node_modules/eslint/node_modules/${request}`);
    }
  }
  return r;
};

module.exports = {
  root: true,
  env: {
    es2022: true,
  },
  extends: ['eslint:recommended', 'plugin:jsdoc/recommended'],
  plugins: ['jsdoc', 'markdown', 'node-core'],
  parser: '@babel/eslint-parser',
  parserOptions: {
    babelOptions: {
      plugins: [
        Module._findPath('@babel/plugin-syntax-import-assertions'),
      ],
    },
    requireConfigFile: false,
    sourceType: 'script',
  },
  overrides: [
    {
      files: [
        'test/es-module/test-esm-type-flag.js',
        'test/es-module/test-esm-type-flag-alias.js',
        '*.mjs',
        'test/es-module/test-esm-example-loader.js',
      ],
      parserOptions: { sourceType: 'module' },
    },
    {
      files: ['**/*.md'],
      processor: 'markdown/markdown',
    },
    {
      files: ['**/*.md/*.cjs', '**/*.md/*.js'],
      parserOptions: {
        sourceType: 'script',
        ecmaFeatures: { impliedStrict: true }
      },
      rules: { strict: 'off' },
    },
    {
      files: [
        '**/*.md/*.mjs',
        'doc/api/esm.md/*.js',
        'doc/api/packages.md/*.js',
      ],
      parserOptions: { sourceType: 'module' },
      rules: { 'no-restricted-globals': [
        'error',
        {
          name: '__filename',
          message: 'Use import.meta.url instead',
        },
        {
          name: '__dirname',
          message: 'Not available in ESM',
        },
        {
          name: 'exports',
          message: 'Not available in ESM',
        },
        {
          name: 'module',
          message: 'Not available in ESM',
        },
        {
          name: 'require',
          message: 'Use import instead',
        },
        {
          name: 'Buffer',
          message: 'Import Buffer instead of using the global'
        },
        {
          name: 'process',
          message: 'Import process instead of using the global'
        },
      ] },
    },
  ],
  rules: {
    // ESLint built-in rules
    // https://eslint.org/docs/rules/
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
    'comma-dangle': ['error', {
      arrays: 'always-multiline',
      exports: 'only-multiline',
      functions: 'only-multiline',
      imports: 'only-multiline',
      objects: 'only-multiline',
    }],
    'comma-spacing': 'error',
    'comma-style': 'error',
    'computed-property-spacing': 'error',
    'default-case-last': 'error',
    'dot-location': ['error', 'property'],
    'dot-notation': 'error',
    'eol-last': 'error',
    'eqeqeq': ['error', 'smart'],
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
      code: 120,
      ignorePattern: '^// Flags:',
      ignoreRegExpLiterals: true,
      ignoreTemplateLiterals: true,
      ignoreUrls: true,
      tabWidth: 2,
    }],
    'new-parens': 'error',
    'no-confusing-arrow': 'error',
    'no-constant-condition': ['error', { checkLoops: false }],
    'no-constructor-return': 'error',
    'no-duplicate-imports': 'error',
    'no-else-return': ['error', { allowElseIf: true }],
    'no-extra-parens': ['error', 'functions'],
    'no-lonely-if': 'error',
    'no-mixed-requires': 'error',
    'no-multi-spaces': ['error', { ignoreEOLComments: true }],
    'no-multiple-empty-lines': ['error', { max: 2, maxEOF: 0, maxBOF: 0 }],
    'no-new-require': 'error',
    'no-path-concat': 'error',
    'no-proto': 'error',
    'no-redeclare': ['error', { 'builtinGlobals': false }],
    'no-restricted-modules': ['error', 'sys'],
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
        selector: "CallExpression[callee.name='isNaN']",
        message: 'Use Number.isNaN() instead of the global isNaN() function.',
      },
    ],
    'no-return-await': 'error',
    'no-self-compare': 'error',
    'no-tabs': 'error',
    'no-template-curly-in-string': 'error',
    'no-throw-literal': 'error',
    'no-trailing-spaces': 'error',
    'no-undef': ['error', { typeof: true }],
    'no-undef-init': 'error',
    'no-unused-expressions': ['error', { allowShortCircuit: true }],
    'no-unused-vars': ['error', { args: 'none', caughtErrors: 'all' }],
    'no-use-before-define': ['error', {
      classes: true,
      functions: false,
      variables: false,
    }],
    'no-useless-call': 'error',
    'no-useless-concat': 'error',
    'no-useless-constructor': 'error',
    'no-useless-return': 'error',
    'no-var': 'error',
    'no-void': 'error',
    'no-whitespace-before-property': 'error',
    'object-curly-newline': 'error',
    'object-curly-spacing': ['error', 'always'],
    'one-var': ['error', { initialized: 'never' }],
    'one-var-declaration-per-line': 'error',
    'operator-linebreak': ['error', 'after'],
    'padding-line-between-statements': [
      'error',
      { blankLine: 'always', prev: 'function', next: 'function' },
    ],
    'prefer-const': ['error', { ignoreReadBeforeAssign: true }],
    'prefer-object-has-own': 'error',
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
    'valid-typeof': ['error', { requireStringLiterals: true }],

    // ESLint recommended rules that we disable
    'no-inner-declarations': 'off',

    // JSDoc recommended rules that we disable
    'jsdoc/require-jsdoc': 'off',
    'jsdoc/require-param-description': 'off',
    'jsdoc/newline-after-description': 'off',
    'jsdoc/require-returns-description': 'off',
    'jsdoc/valid-types': 'off',
    'jsdoc/no-undefined-types': 'off',
    'jsdoc/require-param': 'off',
    'jsdoc/check-tag-names': 'off',
    'jsdoc/require-returns': 'off',
    'jsdoc/require-property-description': 'off',

    // Custom rules from eslint-plugin-node-core
    'node-core/no-unescaped-regexp-dot': 'error',
    'node-core/no-duplicate-requires': 'error',
  },
  globals: {
    ByteLengthQueuingStrategy: 'readable',
    CompressionStream: 'readable',
    CountQueuingStrategy: 'readable',
    Crypto: 'readable',
    CryptoKey: 'readable',
    DecompressionStream: 'readable',
    fetch: 'readable',
    FormData: 'readable',
    ReadableStream: 'readable',
    ReadableStreamDefaultReader: 'readable',
    ReadableStreamBYOBReader: 'readable',
    ReadableStreamBYOBRequest: 'readable',
    ReadableByteStreamController: 'readable',
    ReadableStreamDefaultController: 'readable',
    Response: 'readable',
    TextDecoderStream: 'readable',
    TextEncoderStream: 'readable',
    TransformStream: 'readable',
    TransformStreamDefaultController: 'readable',
    ShadowRealm: 'readable',
    SubtleCrypto: 'readable',
    WritableStream: 'readable',
    WritableStreamDefaultWriter: 'readable',
    WritableStreamDefaultController: 'readable',
  },
};
