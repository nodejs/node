export async function getSource(url, { format }) {
  if (url.endsWith('fixtures/es-modules/message.mjs')) {
    // Oh, I’ve got that one in my cache!
    return {
      source: `export const message = 'Woohoo!'.toUpperCase();`
    }
  }
  return null;
}
