export async function getFormat({ url, defaultGetFormat }) {
  try {
    if (new URL(url).pathname.endsWith('.unknown')) {
      return {
        format: 'module'
      };
    }
  } catch {}
  return defaultGetFormat({url, defaultGetFormat});
}
