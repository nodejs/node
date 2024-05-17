const source = `
  import { test } from 'node:test';
  test('test', async () => {});
`;

export async function load(url, context, nextLoad) {
  if (url.endsWith('virtual.js')) {
    return { format: "module", source, shortCircuit: true };
  }
  return nextLoad(url, context);
}
