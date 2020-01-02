export function getFormat({ url, isMain }, defaultGetFormat, loader) {
  try {
    if (new URL(url).pathname.endsWith('.unknown')) {
      return {format: 'module'};
    }
  } catch {}
  return defaultGetFormat({url, isMain}, defaultGetFormat, loader);
}
