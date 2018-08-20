/* eslint-disable no-restricted-globals */

const globals = new Set(Object.getOwnPropertyNames(global));
globals.delete('undefined');

module.exports = {
  extends: ['../.eslintrc.yaml'],
  rules: {
    'strict': ['error', 'function'],
    'no-restricted-syntax': 'off',
    'no-restricted-globals': [2, ...globals],
  },
};
