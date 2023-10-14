import '../common/index.mjs';
import { strictEqual } from 'assert';

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

{
  const results = await Promise.allSettled([
    import('../fixtures/empty.json', { assert: { type: 'json' } }),
    import('../fixtures/empty.json'),
  ]);

  strictEqual(results[0].status, 'fulfilled');
  strictEqual(results[1].status, 'rejected');
}

{
  const results = await Promise.allSettled([
    import('../fixtures/empty.json'),
    import('../fixtures/empty.json', { assert: { type: 'json' } }),
  ]);

  strictEqual(results[0].status, 'rejected');
  strictEqual(results[1].status, 'fulfilled');
}
