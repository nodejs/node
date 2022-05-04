export async function load(url) {
  const val = url.includes('42')
    ? '42'
    : '"foo"';

  return {
    format: 'module',
    shortCircuit: true,
    source: `export default ${val}`,
  };
}
