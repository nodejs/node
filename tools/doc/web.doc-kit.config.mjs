import { totalmem } from 'node:os';
import { join } from 'node:path';
import { pathToFileURL } from 'node:url';

const hasEnoughMemory = totalmem() > 6 * (1024 ** 3); // 6GB

const fromRoot = (path) =>
  pathToFileURL(join(import.meta.dirname, '..', '..', path)).href;

export default {
  extends: '@node-core/doc-kit/config',

  target: ['legacy-json-all', hasEnoughMemory && 'html'].filter(Boolean),

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
