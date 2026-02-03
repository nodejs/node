import { spawnPromisified } from '../common/index.mjs';
import { describe, it } from 'node:test';
import assert from 'node:assert';

describe('unusual top-level await syntax errors', () => {
  const expressions = [
    // string
    { expression: '""' },
    // number
    { expression: '0' },
    // boolean
    { expression: 'true' },
    // null
    { expression: 'null' },
    // undefined
    { expression: 'undefined' },
    // object
    { expression: '{}' },
    // array
    { expression: '[]' },
    // new
    { expression: 'new Date()' },
    // identifier
    { initialize: 'const a = 2;', expression: 'a' },
  ];
  it('should not crash the process', async () => {
    for (const { expression, initialize } of expressions) {
      const wrapperExpressions = [
        `function callAwait() {}; callAwait(await ${expression});`,
        `if (await ${expression}) {}`,
        `{ key: await ${expression} }`,
        `[await ${expression}]`,
        `(await ${expression})`,
      ];
      for (const wrapperExpression of wrapperExpressions) {
        const { code, signal, stdout, stderr } = await spawnPromisified(process.execPath, [
          '--eval',
          `
          ${initialize || ''}
          ${wrapperExpression}
          `,
        ]);

        assert.strictEqual(stderr, '');
        assert.strictEqual(stdout, '');
        assert.strictEqual(code, 0);
        assert.strictEqual(signal, null);
      }
    }
  });

  it('should throw the error for unrelated syntax errors', async () => {
    const expression = 'foo bar';
    const wrapperExpressions = [
      [`function callSyntaxError() {}; callSyntaxError(${expression});`, /missing \) after argument list/],
      [`if (${expression}) {}`, /Unexpected identifier/],
      [`{ key: ${expression} }`, /Unexpected identifier/],
      [`[${expression}]`, /Unexpected identifier/],
      [`(${expression})`, /Unexpected identifier/],
      [`const ${expression} = 1;`, /Missing initializer in const declaration/],
      [`console.log('PI: ' Math.PI);`, /missing \) after argument list/],
      [`callAwait(await "" "");`, /missing \) after argument list/],
    ];

    for (const [wrapperExpression, error] of wrapperExpressions) {
      const { code, signal, stdout, stderr } = await spawnPromisified(process.execPath, [
        '--eval',
        `
          ${wrapperExpression}
          `,
      ]);
      assert.match(stderr, error);
      assert.strictEqual(stdout, '');
      assert.strictEqual(code, 1);
      assert.strictEqual(signal, null);
    }
  });
});
