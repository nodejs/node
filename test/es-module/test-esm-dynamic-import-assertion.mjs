// Flags: --experimental-json-modules
import '../common/index.mjs';
import { rejects, strictEqual } from 'assert';

const jsModuleDataUrl = 'data:text/javascript,export{}';

await rejects(
  import(`data:text/javascript,import${JSON.stringify(jsModuleDataUrl)}assert{type:"json"}`),
  { code: 'ERR_FAILED_IMPORT_ASSERTION' }
);

await rejects(
  import(jsModuleDataUrl, { assert: { type: 'json' } }),
  { code: 'ERR_FAILED_IMPORT_ASSERTION' }
);

{
  const [secret0, secret1] = await Promise.all([
    import('../fixtures/experimental.json'),
    import(
      '../fixtures/experimental.json',
      { assert: { type: 'json' } }
    ),
  ]);

  strictEqual(secret0.default.ofLife, 42);
  strictEqual(secret1.default.ofLife, 42);
  strictEqual(secret0.default, secret1.default);
  // strictEqual(secret0, secret1);
}

{
  const [secret0, secret1] = await Promise.all([
    import('data:application/json,{"ofLife":42}'),
    import(
      'data:application/json,{"ofLife":42}',
      { assert: { type: 'json' } }
    ),
  ]);

  strictEqual(secret0.default.ofLife, 42);
  strictEqual(secret1.default.ofLife, 42);
}

{
  const results = await Promise.allSettled([
    import('../fixtures/empty.js', { assert: { type: 'json' } }),
    import('../fixtures/empty.js'),
  ]);

  strictEqual(results[0].status, 'rejected');
  strictEqual(results[1].status, 'fulfilled');
}

{
  const results = await Promise.allSettled([
    import('../fixtures/empty.js'),
    import('../fixtures/empty.js', { assert: { type: 'json' } }),
  ]);

  strictEqual(results[0].status, 'fulfilled');
  strictEqual(results[1].status, 'rejected');
}
