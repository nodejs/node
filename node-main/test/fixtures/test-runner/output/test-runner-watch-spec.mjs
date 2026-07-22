import { run } from 'node:test';
import { spec } from 'node:test/reporters';
import tmpdir from '../../../common/tmpdir.js';
import { writeFileSync } from 'node:fs';


const fixtureContent = {
  'dependency.js': 'module.exports = {};',
  'dependency.mjs': 'export const a = 1;',
  'test.js': `
  const test = require('node:test');
  require('./dependency.js');
  import('./dependency.mjs');
  import('data:text/javascript,');
  test('test has ran');`,
  'failing-test.js': `
  const test = require('node:test');
  test('failing test', () => {
    throw new Error('failed');
  });`,
};

tmpdir.refresh();

const fixturePaths = Object.keys(fixtureContent)
  .reduce((acc, file) => ({ ...acc, [file]: tmpdir.resolve(file) }), {});
Object.entries(fixtureContent)
  .forEach(([file, content]) => writeFileSync(fixturePaths[file], content));

const controller = new AbortController();
const { signal } = controller;

const stream = run({
  watch: true,
  cwd: tmpdir.path,
  signal,
});


stream.compose(spec).pipe(process.stdout);

for await (const event of stream) {
  if (event.type === 'test:watch:drained') {
    controller.abort();
  }
}
