import '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import { register } from 'node:module';
import assert from 'node:assert';

async function resolve(referrer, context, next) {
  const result = await next(referrer, context);
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

register(`data:text/javascript,export ${encodeURIComponent(resolve)};export ${encodeURIComponent(load)}`);

assert.notDeepStrictEqual(
  await import(fixtures.fileURL('empty.json'), { with: { type: 'json' } }),
  await import(fixtures.fileURL('empty.json'), { with: { type: 'json' } }),
);
