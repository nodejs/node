import fs from 'node:fs';

export async function resolve(specifier, context, nextResolve) {
  fs.writeSync(1, `resolve ${specifier} from ${context.parentURL}\n`);
  const result = await nextResolve(specifier, context);
  result.shortCircuit = true;
  return result;
}

export async function load(url, context, nextLoad) {
  fs.writeSync(1, `load ${url}\n`);
  const result = await nextLoad(url, context);
  result.shortCircuit = true;
  // Handle the async loader hook quirk where source can be nullish for CommonJS.
  // If this returns that nullish value verbatim, the `require` in imported
  // CJS won't get the hook invoked.
  result.source ??= fs.readFileSync(new URL(url), 'utf8');
  return result;
}
