// Flags: --experimental-loader ./test/fixtures/es-module-loaders/string-sources.mjs
import { mustCall } from '../common/index.mjs';
import assert from 'assert';

assert.rejects(
  import('test:Array'),
  { code: 'ERR_INVALID_RETURN_PROPERTY_VALUE' },
).then(mustCall());
import('test:ArrayBuffer').then(
  mustCall(),
);
import('test:BigInt64Array').then(
  mustCall(),
);
import('test:BigUint64Array').then(
  mustCall(),
);
import('test:Float32Array').then(
  mustCall(),
);
import('test:Float64Array').then(
  mustCall(),
);
import('test:Int8Array').then(
  mustCall(),
);
import('test:Int16Array').then(
  mustCall(),
);
import('test:Int32Array').then(
  mustCall(),
);
assert.rejects(
  import('test:null'),
  { code: 'ERR_INVALID_RETURN_PROPERTY_VALUE' },
).then(mustCall());
assert.rejects(
  import('test:Object'),
  { code: 'ERR_INVALID_RETURN_PROPERTY_VALUE' },
).then(mustCall());
import('test:SharedArrayBuffer').then(
  mustCall(),
);
import('test:string').then(
  mustCall(),
);
assert.rejects(
  import('test:String'),
  { code: 'ERR_INVALID_RETURN_PROPERTY_VALUE' },
).then(mustCall());
import('test:Uint8ClampedArray').then(
  mustCall(),
);
import('test:Uint16Array').then(
  mustCall(),
);
import('test:Uint32Array').then(
  mustCall(),
);
import('test:Uint8Array').then(
  mustCall(),
);
assert.rejects(
  import('test:undefined'),
  { code: 'ERR_INVALID_RETURN_PROPERTY_VALUE' },
).then(mustCall());
