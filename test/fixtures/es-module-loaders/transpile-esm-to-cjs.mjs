import { writeSync } from "node:fs";

export async function resolve(specifier, context, next) {
  const result = await next(specifier, context);
  if (specifier.startsWith("file://")) {
    writeSync(1, `Resolved format: ${result.format}\n`);
  }
  return result;
}

export async function load(url, context, next) {
  const output = await next(url, context);
  writeSync(1, `Loaded original format: ${output.format}\n`);

  let source = `${output.source}`

  // This is a very incomplete and naively done implementation for testing purposes only
  if (source?.includes('export default')) {
    source = source.replace('export default', 'module.exports =');

    source += '\nconsole.log(`Evaluated format: ${this === undefined ? "module" : "commonjs"}`);';

    output.source = source;
    output.format = 'commonjs';
  }

  return output;
}
