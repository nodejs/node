import '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import { registerHooks } from 'node:module';
import assert from 'node:assert';

function resolve(referrer, context, next) {
  const result = next(referrer, context);
  const url = new URL(result.url);
  url.searchParams.set('randomSeed', Math.random());
  result.url = url.href;
  return result;
}

function load(url, context, next) {
  if (context.importAttributes.type === 'json') {
    return {
      shortCircuit: true,
      format: 'json',
      source: JSON.stringify({ data: Math.random() }),
    };
  }
  return next(url, context);
}

registerHooks({ resolve, load });

assert.notDeepStrictEqual(
  await import(fixtures.fileURL('empty.json'), { with: { type: 'json' } }),
  await import(fixtures.fileURL('empty.json'), { with: { type: 'json' } }),
);
