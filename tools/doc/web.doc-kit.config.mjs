// @node-core/doc-kit configuration for building the API documentation
// (out/doc/api): the HTML site, the JSON consumed by other tooling, and
// `llms.txt`.

import { fileURLToPath } from 'node:url';

const root = new URL('../../', import.meta.url);

const fromRoot = (path) =>
  fileURLToPath(new URL(path, root)).replaceAll('\\', '/');

const urlFromRoot = (path) => new URL(path, root).href;

export default {
  extends: '@node-core/doc-kit/config',

  target: [
    'legacy-json-all',
    'section-pages',
  ],

  global: {
    // Files to skip are passed by the Makefile (`skip_apidoc_files`).
    input: [fromRoot('doc/api/*.md')],
    output: fromRoot('out/doc/api'),

    // Point every loadable URL at its local file so no network request is made.
    changelog: urlFromRoot('CHANGELOG.md'),
    index: urlFromRoot('doc/api/index.md'),
  },

  metadata: {
    typeMap: urlFromRoot('doc/type-map.json'),
  },
};
