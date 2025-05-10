import {
  globals,
  noRestrictedSyntaxCommonAll,
  noRestrictedSyntaxCommonLib,
} from '../tools/eslint/eslint.config_utils.mjs';
import { builtinModules } from 'node:module';

const builtin = builtinModules.filter((name) => !name.startsWith('node:'));

export default [
  {
    files: ['doc/**/*.md/*.{js,mjs,cjs,ts,cts,mts}'],
    rules: {
      // Ease some restrictions in doc examples.
      'no-restricted-properties': 'off',
      'no-restricted-syntax': [
        'error',
        ...noRestrictedSyntaxCommonAll,
        ...noRestrictedSyntaxCommonLib,
        {
          selector: `CallExpression[callee.name="require"][arguments.0.type="Literal"]:matches(${builtin.map((name) => `[arguments.0.value="${name}"]`).join(',')}),ImportDeclaration:matches(${builtin.map((name) => `[source.value="${name}"]`).join(',')})`,
          message: 'Use `node:` prefix.',
        },
      ],

      // Typescript rules
      '@typescript-eslint/no-unused-expressions': 'off',
      '@typescript-eslint/no-unused-vars': 'off',
      // Removed since it is used on how not to use examples.
      '@typescript-eslint/no-namespace': 'off',

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
