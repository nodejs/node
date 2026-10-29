export async function load(url, context, nextLoad) {
  return nextLoad(url, context);
}
