// @node-core/doc-kit configuration for generating and linting the Node.js
// man-page (doc/node.1).

import { fileURLToPath } from 'node:url';

const root = new URL('../../', import.meta.url);

// POSIX separators, because `input` is a glob pattern and a backslash reads as
// an escape character, which would mangle Windows paths into non-matches.
const fromRoot = (path) =>
  fileURLToPath(new URL(path, root)).replaceAll('\\', '/');

// doc-kit only treats a value as local when it parses as a `file:` URL, and a
// Windows drive letter parses as a URL scheme, so these must stay URLs.
const urlFromRoot = (path) => new URL(path, root).href;

export default {
  target: ['man-page'],

  global: {
    input: [fromRoot('doc/api/cli.md')],
    output: fromRoot('tools/doc/.manpagecheck'),

    // Point every loadable URL at its local file so no network request is made.
    changelog: urlFromRoot('CHANGELOG.md'),
    index: urlFromRoot('doc/api/index.md'),
  },
};
