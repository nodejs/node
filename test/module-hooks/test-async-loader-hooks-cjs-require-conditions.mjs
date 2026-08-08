import '../common/index.mjs';
import assert from 'node:assert';
import { register } from 'node:module';

const loader = `
  import assert from 'node:assert';

  function assertCjsConditions(context) {
    assert.ok(
      context.conditions.includes('require'),
      \`Conditions should include "require": \${JSON.stringify(context.conditions)}\`,
    );
    assert.ok(
      !context.conditions.includes('import'),
      \`Conditions should not include "import": \${JSON.stringify(context.conditions)}\`,
    );
  }

  export async function resolve(specifier, context, nextResolve) {
    if (specifier === 'custom:cjs') {
      return { url: 'custom:cjs', format: 'commonjs', shortCircuit: true };
    }

    if (specifier === 'node:target') {
      assertCjsConditions(context);
      return { url: 'node:fs', shortCircuit: true };
    }

    return nextResolve(specifier, context);
  }

  export async function load(url, context, nextLoad) {
    if (url === 'custom:cjs') {
      return {
        format: 'commonjs',
        source: \`
          module.exports = require.resolve('node:target');
        \`,
        shortCircuit: true,
      };
    }

    return nextLoad(url, context);
  }
`;

register(`data:text/javascript,${encodeURIComponent(loader)}`);

const ns = await import('custom:cjs');
assert.strictEqual(ns.default, 'node:fs');
