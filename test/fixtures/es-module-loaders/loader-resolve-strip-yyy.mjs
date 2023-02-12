export async function resolve(specifier, context, nextResolve) {
  console.log(`loader-b`, {specifier});
  return nextResolve(specifier.replace(/^yyy\//, `./`));
}
