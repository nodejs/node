export async function resolve(specifier, context, nextResolve) {
  if (specifier === 'pkg' && !context.conditions.includes('worker')) {
    throw new Error(`missing worker condition: ${context.conditions}`);
  }
  return nextResolve(specifier, context);
}
