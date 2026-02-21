import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';

export async function load(url, context, nextLoad) {
  const result = await nextLoad(url, context);

  if (result.format === 'commonjs' && !result.source) {
    result.source = readFileSync(fileURLToPath(url), 'utf8');
  }

  return result;
}
