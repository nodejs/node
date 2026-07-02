// @node-core/doc-kit configuration for generating and linting the Node.js
// man-page (doc/node.1).

import { fileURLToPath } from 'node:url';

const root = new URL('../../', import.meta.url);

const fromRoot = (path) => fileURLToPath(new URL(path, root));

export default {
  target: ['man-page'],

  global: {
    input: [fromRoot('doc/api/cli.md')],
    output: fromRoot('out/doc/.manpagecheck'),

    // Point every loadable URL at its local file so no network request is made.
    changelog: fromRoot('CHANGELOG.md'),
    index: fromRoot('doc/api/index.md'),
  },
};
