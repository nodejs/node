import { fileURLToPath } from 'node:url';

const root = new URL('../../', import.meta.url);

const fromRoot = (path) =>
  fileURLToPath(new URL(path, root)).replaceAll('\\', '/');

const urlFromRoot = (path) => new URL(path, root).href;

export default {
  extends: '@node-core/doc-kit/config',

  target: ['api-links'],

  global: {
    input: [fromRoot('lib/*.js')],
    output: fromRoot('out/doc'),

    // Point every loadable URL at its local file so no network request is made.
    changelog: urlFromRoot('CHANGELOG.md'),
  },
};
