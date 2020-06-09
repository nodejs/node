import '../common/index.mjs';
import { strictEqual } from 'assert';

// Flags: --experimental-loader ./test/fixtures/es-module-loaders/loader-chain-a.mjs --experimental-loader ./test/fixtures/es-module-loaders/loader-chain-b.mjs

// loader-chain-a.mjs changes AAA to BBB
// loader-chain-b.mjs changes BBB to CCC
strictEqual('AAA', 'CCC');
