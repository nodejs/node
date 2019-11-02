// Flags:  --experimental-modules --experimental-loader i-dont-exist
import '../common/index.mjs';
console.log('This should not be printed');
