export function load(url, context, nextLoad) {
  switch (url) {
    case 'uyyt://1/index.mjs':
      return {
        source: 'console.log("index.mjs!")',
        format: 'module',
        shortCircuit: true,
      };
    case 'uyyt://1/index2.mjs':
      return {
        source: 'import c from "./sub.mjs"; console.log(c);',
        format: 'module',
        shortCircuit: true,
      };
    case 'uyyt://1/sub.mjs':
      return {
        source: 'export default 42',
        format: 'module',
        shortCircuit: true,
      };
    default:
      return nextLoad(url, context);
  }
}
