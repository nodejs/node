import { createRequire } from 'node:module';
import { totalmem } from 'node:os';
import { join } from 'node:path';
import { pathToFileURL } from 'node:url';

const require = createRequire(import.meta.url);

// Gate HTML generation for high-memory machines
//
// TODO(@avivkeller): Lower the amount of memory
// we use.
const hasEnoughMemory = totalmem() > 5 * (1024 ** 3);

// The HTML generator bundles its CSS with Lightning CSS, which ships as a
// native binding that is not available on every platform we build on.
// Probe for it up front and skip HTML generation when it cannot be loaded,
// rather than failing partway through the build.
//
// TODO(@avivkeller): Fall back to WASM on machines
// without native implementations of our dependencies
const canLoadLightningCSS = () => {
  try {
    require('lightningcss');
    return true;
  } catch (error) {
    console.warn(`Skipping HTML generation: unable to load lightningcss (${error.message.split('\n')[0]})`);
    return false;
  }
};

const fromRoot = (path) =>
  pathToFileURL(join(import.meta.dirname, '..', '..', path)).href;

export default {
  extends: '@node-core/doc-kit/config',

  target: [
    'json-all',
    hasEnoughMemory && canLoadLightningCSS() && 'section-pages',
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
