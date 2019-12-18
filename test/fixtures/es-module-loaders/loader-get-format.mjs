export async function getFormat({url, isMain}, defaultGetFormat) {
  try {
    if (new URL(url).pathname.endsWith('.unknown')) {
      return {format: 'module'};
    }
  } catch (e) {
  }
  return defaultGetFormat({url, isMain});
}
