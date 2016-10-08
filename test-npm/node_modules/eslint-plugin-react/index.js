'use strict';

module.exports = {
  rules: {
    'jsx-uses-react': require('./lib/rules/jsx-uses-react'),
    'no-multi-comp': require('./lib/rules/no-multi-comp'),
    'prop-types': require('./lib/rules/prop-types'),
    'display-name': require('./lib/rules/display-name'),
    'wrap-multilines': require('./lib/rules/wrap-multilines'),
    'self-closing-comp': require('./lib/rules/self-closing-comp'),
    'no-danger': require('./lib/rules/no-danger'),
    'no-set-state': require('./lib/rules/no-set-state'),
    'no-is-mounted': require('./lib/rules/no-is-mounted'),
    'no-deprecated': require('./lib/rules/no-deprecated'),
    'no-did-mount-set-state': require('./lib/rules/no-did-mount-set-state'),
    'no-did-update-set-state': require('./lib/rules/no-did-update-set-state'),
    'react-in-jsx-scope': require('./lib/rules/react-in-jsx-scope'),
    'jsx-uses-vars': require('./lib/rules/jsx-uses-vars'),
    'jsx-handler-names': require('./lib/rules/jsx-handler-names'),
    'jsx-pascal-case': require('./lib/rules/jsx-pascal-case'),
    'jsx-no-bind': require('./lib/rules/jsx-no-bind'),
    'jsx-no-undef': require('./lib/rules/jsx-no-undef'),
    'no-unknown-property': require('./lib/rules/no-unknown-property'),
    'jsx-curly-spacing': require('./lib/rules/jsx-curly-spacing'),
    'jsx-equals-spacing': require('./lib/rules/jsx-equals-spacing'),
    'jsx-sort-props': require('./lib/rules/jsx-sort-props'),
    'sort-prop-types': require('./lib/rules/sort-prop-types'),
    'jsx-sort-prop-types': require('./lib/rules/jsx-sort-prop-types'),
    'jsx-boolean-value': require('./lib/rules/jsx-boolean-value'),
    'sort-comp': require('./lib/rules/sort-comp'),
    'require-extension': require('./lib/rules/require-extension'),
    'jsx-no-duplicate-props': require('./lib/rules/jsx-no-duplicate-props'),
    'jsx-max-props-per-line': require('./lib/rules/jsx-max-props-per-line'),
    'jsx-no-literals': require('./lib/rules/jsx-no-literals'),
    'jsx-indent-props': require('./lib/rules/jsx-indent-props'),
    'jsx-indent': require('./lib/rules/jsx-indent'),
    'jsx-closing-bracket-location': require('./lib/rules/jsx-closing-bracket-location'),
    'jsx-space-before-closing': require('./lib/rules/jsx-space-before-closing'),
    'no-direct-mutation-state': require('./lib/rules/no-direct-mutation-state'),
    'forbid-prop-types': require('./lib/rules/forbid-prop-types'),
    'prefer-es6-class': require('./lib/rules/prefer-es6-class'),
    'jsx-key': require('./lib/rules/jsx-key'),
    'no-string-refs': require('./lib/rules/no-string-refs'),
    'prefer-stateless-function': require('./lib/rules/prefer-stateless-function'),
    'require-render-return': require('./lib/rules/require-render-return')
  },
  configs: {
    recommended: {
      parserOptions: {
        ecmaFeatures: {
          jsx: true
        }
      },
      rules: {
        'react/display-name': 2,
        'react/jsx-no-duplicate-props': 2,
        'react/jsx-no-undef': 2,
        'react/jsx-uses-react': 2,
        'react/jsx-uses-vars': 2,
        'react/no-danger': 2,
        'react/no-deprecated': 2,
        'react/no-did-mount-set-state': [2, 'allow-in-func'],
        'react/no-did-update-set-state': [2, 'allow-in-func'],
        'react/no-direct-mutation-state': 2,
        'react/no-is-mounted': 2,
        'react/no-unknown-property': 2,
        'react/prop-types': 2,
        'react/react-in-jsx-scope': 2
      }
    }
  }
};
