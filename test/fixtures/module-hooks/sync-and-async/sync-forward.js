export async function load(url, context, nextLoad) {
  const result = await nextLoad(url, context);
  if (!result) {
    return {
      format: 'esm',
      source: 'console.log("Hello world");', // fallback source
    };
  }
  return result;
}
