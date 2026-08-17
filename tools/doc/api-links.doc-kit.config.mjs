import { join } from 'node:path';
import { pathToFileURL } from 'node:url';

const fromRoot = (path) =>
  pathToFileURL(join(import.meta.dirname, '..', '..', path)).href;

export default {
  extends: '@node-core/doc-kit/config',

  target: ['api-links'],

  global: {
    input: ['lib/*.js'],

    changelog: fromRoot('CHANGELOG.md'),
  },
};
