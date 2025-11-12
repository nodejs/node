export async function load(url, context, nextLoad) {
  const result = await nextLoad(url, context);

  // If nextLoad returned null, provide fallback
  if (!result) {
    return {
      format: 'esm',
      source: 'console.log("Hello world");',
    };
  }

  return result;
}
