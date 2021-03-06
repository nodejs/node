export async function getFormat(url, context, defaultGetFormat) {
  try {
    if (new URL(url).pathname.endsWith('.unknown')) {
      return {
        format: 'module'
      };
    }
  } catch {}
  return defaultGetFormat(url, context, defaultGetFormat);
}
