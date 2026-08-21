// Flags: --experimental-loader ./test/fixtures/es-module-loaders/string-sources.mjs
import { mustCall, mustNotCall } from '../common/index.mjs';
import assert from 'assert';

import('test:Array').then(
  mustNotCall('Should not accept Arrays'),
  mustCall((e) => {
    assert.strictEqual(e.code, 'ERR_INVALID_RETURN_PROPERTY_VALUE');
  })
);
import('test:ArrayBuffer').then(
  mustCall(),
  mustNotCall('Should accept ArrayBuffers'),
);
import('test:BigInt64Array').then(
  mustCall(),
  mustNotCall('Should accept BigInt64Array'),
);
import('test:BigUint64Array').then(
  mustCall(),
  mustNotCall('Should accept BigUint64Array'),
);
import('test:Float32Array').then(
  mustCall(),
  mustNotCall('Should accept Float32Array'),
);
import('test:Float64Array').then(
  mustCall(),
  mustNotCall('Should accept Float64Array'),
);
import('test:Int8Array').then(
  mustCall(),
  mustNotCall('Should accept Int8Array'),
);
import('test:Int16Array').then(
  mustCall(),
  mustNotCall('Should accept Int16Array'),
);
import('test:Int32Array').then(
  mustCall(),
  mustNotCall('Should accept Int32Array'),
);
import('test:null').then(
  mustNotCall('Should not accept null'),
  mustCall((e) => {
    assert.strictEqual(e.code, 'ERR_INVALID_RETURN_PROPERTY_VALUE');
  })
);
import('test:Object').then(
  mustNotCall('Should not stringify or valueOf Objects'),
  mustCall((e) => {
    assert.strictEqual(e.code, 'ERR_INVALID_RETURN_PROPERTY_VALUE');
  })
);
import('test:SharedArrayBuffer').then(
  mustCall(),
  mustNotCall('Should accept SharedArrayBuffers'),
);
import('test:string').then(
  mustCall(),
  mustNotCall('Should accept strings'),
);
import('test:String').then(
  mustNotCall('Should not accept wrapper Strings'),
  mustCall((e) => {
    assert.strictEqual(e.code, 'ERR_INVALID_RETURN_PROPERTY_VALUE');
  })
);
import('test:Uint8ClampedArray').then(
  mustCall(),
  mustNotCall('Should accept Uint8ClampedArray'),
);
import('test:Uint16Array').then(
  mustCall(),
  mustNotCall('Should accept Uint16Array'),
);
import('test:Uint32Array').then(
  mustCall(),
  mustNotCall('Should accept Uint32Array'),
);
import('test:Uint8Array').then(
  mustCall(),
  mustNotCall('Should accept Uint8Arrays'),
);
import('test:undefined').then(
  mustNotCall('Should not accept undefined'),
  mustCall((e) => {
    assert.strictEqual(e.code, 'ERR_INVALID_RETURN_PROPERTY_VALUE');
  })
);
