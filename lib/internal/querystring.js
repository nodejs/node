'use strict';

const hexTable = new Array(256);
for (var i = 0; i < 256; ++i)
  hexTable[i] = '%' + ((i < 16 ? '0' : '') + i.toString(16)).toUpperCase();

// Instantiating this is faster than explicitly calling `Object.create(null)`
// to get a "clean" empty object (tested with v8 v4.9).
function StorageObject() {}
StorageObject.prototype = Object.create(null);

module.exports = {
  hexTable,
  StorageObject
};
