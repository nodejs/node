import { readFile } from 'node:fs/promises';

export async function load(url, context, nextLoad) {
  const resolved = await nextLoad(url, context);
  if (context.format === 'commonjs') {
    resolved.source = await readFile(new URL(url));
  }
  return resolved;
}
