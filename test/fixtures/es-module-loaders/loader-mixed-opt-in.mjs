import { createRequire } from 'module';

const require = createRequire(import.meta.url);
const fixtures = require('../../common/fixtures.js');

const fixturesPath = fixtures.path('empty.js');

export function resolve(s, c, n) {
  if (s.endsWith('entry-point')) {
    return {
      shortCircuit: true,
      url: 'file:///c:/virtual-entry-point',
      format: 'commonjs',
    };
  }
  return n(s, c);
}

export async function load(u, c, n) {
  if (u === 'file:///c:/virtual-entry-point') {
    return {
      shortCircuit: true,
      source: `"use strict";require(${JSON.stringify(fixturesPath)});console.log("Hello");`,
      format: 'commonjs',
    };
  }
  return n(u, c);
}
