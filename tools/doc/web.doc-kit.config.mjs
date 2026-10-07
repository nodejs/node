import { createRequire } from 'node:module';
import { totalmem } from 'node:os';
import { join } from 'node:path';
import { pathToFileURL } from 'node:url';

const require = createRequire(import.meta.url);

const fromRoot = (path) =>
  pathToFileURL(join(import.meta.dirname, '..', '..', path)).href;

export default {
  extends: '@node-core/doc-kit/config',

  target: [
    'legacy-json-all',
    'legacy-html-all'
  ].filter(Boolean),

  global: {
    input: ['doc/api/*.md'],
    ignore: ['doc/api/quic.md'],
    output: 'out/doc/api',

    changelog: fromRoot('CHANGELOG.md'),
  },

  metadata: {
    typeMap: fromRoot('doc/type-map.json'),
  },
};
