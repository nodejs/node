export async function resolve(specifier, context, nextResolve) {
  console.log(`loader-a`, {specifier});
  return nextResolve(specifier.replace(/^xxx\//, `./`));
}
