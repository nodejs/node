// Flags: --expose-internals
import { mustCall } from '../common/index.mjs';
import esmLoaderModule from 'internal/modules/esm/loader';
import assert from 'assert';

const { ESMLoader } = esmLoaderModule;

{
  const esmLoader = new ESMLoader();

  const originalSpecifier = 'foo/bar';
  const importAssertions = { type: 'json' };
  const parentURL = 'file:///entrypoint.js';
  const resolvedURL = 'file:///foo/bar.js';
  const suggestedFormat = 'test';

  function resolve(specifier, context, defaultResolve) {
    assert.strictEqual(specifier, originalSpecifier);
    assert.deepStrictEqual(Object.keys(context), [
      'conditions',
      'importAssertions',
      'parentURL',
    ]);
    assert.ok(Array.isArray(context.conditions));
    assert.strictEqual(context.importAssertions, importAssertions);
    assert.strictEqual(context.parentURL, parentURL);
    assert.strictEqual(typeof defaultResolve, 'function');

    return {
      format: suggestedFormat,
      url: resolvedURL,
    };
  }

  function load(resolvedURL, context, defaultLoad) {
    assert.strictEqual(resolvedURL, resolvedURL);
    assert.ok(new URL(resolvedURL));
    assert.deepStrictEqual(Object.keys(context), [
      'format',
      'importAssertions',
    ]);
    assert.strictEqual(context.format, suggestedFormat);
    assert.strictEqual(context.importAssertions, importAssertions);
    assert.strictEqual(typeof defaultLoad, 'function');

    // This doesn't matter (just to avoid errors)
    return {
      format: 'module',
      source: '',
    };
  }

  const customLoader1 = {
    resolve: mustCall(resolve),
    load: mustCall(load),
  };

  esmLoader.addCustomLoaders(customLoader1);

  esmLoader.resolve(
    originalSpecifier,
    parentURL,
    importAssertions,
  );
  esmLoader.load(
    resolvedURL,
    {
      format: suggestedFormat,
      importAssertions,
    },
    function mockDefaultLoad() {},
  );
}
