export async function load(url, context, nextLoad) {
  if (url.endsWith('app.js')) {
    return {
      shortCircuit: true,
      format: 'module',
      source: 'console.log("customized by async hook");',
    };
  }
  return nextLoad(url, context);
}
