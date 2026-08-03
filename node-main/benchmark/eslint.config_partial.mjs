import { globals } from '../tools/eslint/eslint.config_utils.mjs';

export default [
  {
    files: ['benchmark/**/*.{js,mjs,cjs}'],
    languageOptions: {
      globals: {
        ...globals.node,
      },
    },
    rules: {
      'prefer-arrow-callback': 'error',
    },
  },
];
