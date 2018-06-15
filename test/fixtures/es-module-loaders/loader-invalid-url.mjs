export async function resolve(specifier, parentModuleURL, defaultResolve) {
  if (parentModuleURL && specifier !== 'assert')
    return {
      url: specifier,
      format: 'esm'
    };
  return defaultResolve(specifier, parentModuleURL);
}
