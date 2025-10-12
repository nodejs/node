'use strict'

const neo = require('neostandard')
const { installedExports } = require('./lib/global')

module.exports = [
  ...neo({
    ignores: [
      'lib/llhttp',
      'test/fixtures/cache-tests',
      'undici-fetch.js',
      'test/web-platform-tests/wpt'
    ],
    noJsx: true,
    ts: true
  }),
  {
    rules: {
      '@stylistic/comma-dangle': ['error', {
        arrays: 'never',
        objects: 'never',
        imports: 'never',
        exports: 'never',
        functions: 'never'
      }],
      '@typescript-eslint/no-redeclare': 'off',
      'no-restricted-globals': ['error',
        ...installedExports.map(name => {
          return {
            name,
            message: `Use undici-own ${name} instead of the global.`
          }
        })
      ]
    }
  }
]
