import { requireEslintTool } from '../tools/eslint.config_utils.mjs';

const globals = requireEslintTool('globals');

export default [
  {
    files: ['doc/**/*.md/*.{js,mjs,cjs}'],
    rules: {
      // Ease some restrictions in doc examples.
      'no-restricted-properties': 'off',
      'no-undef': 'off',
      'no-unused-expressions': 'off',
      'no-unused-vars': 'off',
      'symbol-description': 'off',

      // Add new ECMAScript features gradually.
      'prefer-const': 'error',
      'prefer-rest-params': 'error',
      'prefer-template': 'error',

      // Stylistic rules.
      '@stylistic/js/no-multiple-empty-lines': [
        'error',
        {
          max: 1,
          maxEOF: 0,
          maxBOF: 0,
        },
      ],
    },
  },
  {
    files: ['doc/api_assets/*.js'],
    languageOptions: {
      globals: {
        ...globals.browser,
      },
    },
  },
];
