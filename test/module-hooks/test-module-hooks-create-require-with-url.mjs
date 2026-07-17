// Verify that if URL is used to createRequire, that URL is passed to the resolve hook
// as parentURL.
import * as common from '../common/index.mjs';
import assert from 'node:assert';
import { registerHooks } from 'node:module';
import * as fixtures from '../common/fixtures.mjs';

const fixtureURL = fixtures.fileURL('module-hooks/create-require-with-url.mjs').href + '?test=1';
registerHooks({
  resolve: common.mustCall((specifier, context, defaultResolve) => {
    const resolved = defaultResolve(specifier, context, defaultResolve);
    if (specifier.startsWith('node:')) {
      return resolved;
    }

    if (specifier === fixtureURL) {
      assert.strictEqual(context.parentURL, import.meta.url);
    } else {  // From the createRequire call.
      assert.strictEqual(specifier, './empty.mjs');
      assert.strictEqual(context.parentURL, fixtureURL);
    }
    return resolved;
  }, 3),
});

await import(fixtureURL);
