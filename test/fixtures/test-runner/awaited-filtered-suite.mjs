import test from 'node:test';

await test.suite('Suite A', async () => {});

await test.suite('Suite B', async () => {
  await test('Test 1', async () => {});
  await test('Test 2', async () => {});
});

await test.suite('Suite C', async () => {
  await test('Test 3', async () => {});
});
