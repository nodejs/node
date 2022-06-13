'use strict';

const fs = require('fs');
const zlib = require('zlib');
const path = require('path');
const assert = require('assert');

const {
  isBuildingSnapshot,
  addSerializeCallback,
  addDeserializeCallback,
  setDeserializeMainFunction
} = require('v8').startupSnapshot;

const filePath = path.resolve(__dirname, '../x1024.txt');
const storage = {};

assert(isBuildingSnapshot());

addSerializeCallback(({ filePath }) => {
  console.error('serializing', filePath);
  storage[filePath] = zlib.gzipSync(fs.readFileSync(filePath));
}, { filePath });

addDeserializeCallback(({ filePath }) => {
  console.error('deserializing', filePath);
  storage[filePath] = zlib.gunzipSync(storage[filePath]);
}, { filePath });

setDeserializeMainFunction(({ filePath }) => {
  console.log(storage[filePath].toString());
}, { filePath });
