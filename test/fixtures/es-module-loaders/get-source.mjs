export async function loadFromURL(url, context, defaultLoadFromURL) {
  if (url.endsWith('fixtures/es-modules/message.mjs')) {
    // Oh, I’ve got that one in my cache!
    return {
      format: 'module',
      source: `export const message = 'Woohoo!'.toUpperCase();`
    }
  } else {
    return defaultLoadFromURL(url, context, defaultLoadFromURL);
  }
}
