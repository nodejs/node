let importedESM = 0;
let importedCJS = 0;
global.getModuleTypeStats = () => { return {importedESM, importedCJS} };

export async function load(url, context, next) {
  return next(url, context, next);
}

export async function resolve(specifier, context, next) {
  const nextResult = await next(specifier, context);
  const { format } = nextResult;

  if (format === 'module' || specifier.endsWith('.mjs')) {
    importedESM++;
  } else if (format == null || format === 'commonjs') {
    importedCJS++;
  }

  return nextResult;
}

