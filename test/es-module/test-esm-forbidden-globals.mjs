import '../common/index.mjs';

// eslint-disable-next-line no-undef
if (typeof arguments !== 'undefined') {
  throw new Error('not an ESM');
}
if (typeof this !== 'undefined') {
  throw new Error('not an ESM');
}
if (typeof exports !== 'undefined') {
  throw new Error('not an ESM');
}
if (typeof require !== 'undefined') {
  throw new Error('not an ESM');
}
if (typeof module !== 'undefined') {
  throw new Error('not an ESM');
}
if (typeof __filename !== 'undefined') {
  throw new Error('not an ESM');
}
if (typeof __dirname !== 'undefined') {
  throw new Error('not an ESM');
}
