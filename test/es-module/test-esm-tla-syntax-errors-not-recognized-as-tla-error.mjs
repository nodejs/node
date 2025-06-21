import { spawnPromisified } from '../common/index.mjs';
import { describe, it } from 'node:test';
import { strictEqual, match } from 'node:assert';

describe('maybe top-level await syntax errors that are not recognized as top-level await errors', () => {
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

        strictEqual(stderr, '');
        strictEqual(stdout, '');
        strictEqual(code, 0);
        strictEqual(signal, null);
      }
    }
  });

  it('should crash when the expression is not valid', async () => {
    let { code, signal, stdout, stderr } = await spawnPromisified(process.execPath, [
      '--eval',
      `
      function callAwait() {}
      callAwait(await "" ""); 
      `,
    ]);
    match(stderr, /SyntaxError: missing \) after argument list/);
    strictEqual(stdout, '');
    strictEqual(code, 1);
    strictEqual(signal, null);

    ({ code, signal, stdout, stderr } = await spawnPromisified(process.execPath, [
      '--eval',
      `
      function callAwait() {}
      if (a "") {}
      `,
    ]));
    match(stderr, /SyntaxError: Unexpected string/);
    strictEqual(stdout, '');
    strictEqual(code, 1);
    strictEqual(signal, null);
  });
});
