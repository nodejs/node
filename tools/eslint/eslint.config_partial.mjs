import { globals } from './eslint.config_utils.mjs';

export default [
  {
    files: ['tools/**/*.{js,mjs,cjs}'],
    languageOptions: {
      globals: {
        ...globals.node,
      },
    },
    rules: {
      'camelcase': [
        'error',
        {
          properties: 'never',
          ignoreDestructuring: true,
          allow: ['child_process'],
        },
      ],
      'no-unused-vars': [
        'error',
        { args: 'after-used' },
      ],
      'prefer-arrow-callback': 'error',
    },
  },
];
