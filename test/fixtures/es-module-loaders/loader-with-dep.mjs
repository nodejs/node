import {createRequire} from '../../common/index.mjs';

const require = createRequire(import.meta.url);
const dep = require('./loader-dep.js');

export async function resolve(specifier, { parentURL, importAttributes }, defaultResolve) {
  return {
    url: (await defaultResolve(specifier, { parentURL, importAttributes }, defaultResolve)).url,
    format: dep.format
  };
}
