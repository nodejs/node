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
