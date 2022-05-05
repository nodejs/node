export async function resolve(specifier) {
  return {
    shortCircuit: true,
    url: specifier,
  }
}
