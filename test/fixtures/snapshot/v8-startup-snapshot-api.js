'use strict';

const fs = require('fs');
const zlib = require('zlib');
const path = require('path');
const {
  isBuildingSnapshot,
  addSerializeCallback,
  addDeserializeCallback,
  setDeserializeMainFunction
} = require('v8').startupSnapshot;

const filePath = path.resolve(__dirname, '../x1024.txt');
const storage = {};

if (isBuildingSnapshot()) {
  addSerializeCallback(({ filePath }) => {
    storage[filePath] = zlib.gzipSync(fs.readFileSync(filePath));
  }, { filePath });

  addDeserializeCallback(({ filePath }) => {
    storage[filePath] = zlib.gunzipSync(storage[filePath]);
  }, { filePath });

  setDeserializeMainFunction(({ filePath }) => {
    console.log(storage[filePath].toString());
  }, { filePath });
}
