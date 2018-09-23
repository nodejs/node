'use strict';

const resolve = require('rollup-plugin-node-resolve');
const commonjs = require('rollup-plugin-commonjs');
const json = require('rollup-plugin-json');

module.exports = {
  input: 'src/cli-entry.js',
  output: {
    file: 'dist/lint-md.js',
    format: 'cjs',
    sourcemap: false
  },
  external: [
    'stream',
    'path',
    'module',
    'util',
    'tty',
    'os',
    'fs',
    'events',
    'assert'
  ],
  plugins: [
    {
      name: 'brute-replace',
      transform(code, id) {
        const normID = id.replace(__dirname, '').replace(/\\+/g, '/');
        if (normID === '/node_modules/concat-stream/index.js') {
          return code.replace('\'readable-stream\'', '\'stream\'');
        }
        if (normID === '/node_modules/unified-args/lib/options.js') {
          return code.replace('\'./schema\'', '\'./schema.json\'');
        }
      }
    },
    json({
      preferConst: true
    }),
    resolve(), // tells Rollup how to find date-fns in node_modules
    commonjs(), // converts date-fns to ES modules
  ]
};
