export async function getFormat(url) {
  try {
    if (new URL(url).pathname.endsWith('.unknown')) {
      return {
        format: 'module'
      };
    }
  } catch {}
  return null;
}
