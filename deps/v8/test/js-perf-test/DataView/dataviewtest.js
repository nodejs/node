// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

new BenchmarkSuite('DataViewTest-DataView-BigEndian', [1000], [
  new Benchmark('DataViewTest-DataView-BigEndian', false, false, 0, doTestDataViewBigEndian),
]);

new BenchmarkSuite('DataViewTest-DataView-LittleEndian', [1000], [
  new Benchmark('DataViewTest-DataView-LittleEndian', false, false, 0, doTestDataViewLittleEndian),
]);

new BenchmarkSuite('DataViewTest-DataView-Floats', [1000], [
  new Benchmark('DataViewTest-DataView-Floats', false, false, 0, doTestDataViewFloats),
]);

new BenchmarkSuite('DataViewTest-TypedArray-BigEndian', [1000], [
  new Benchmark('DataViewTest-TypedArray-BigEndian', false, false, 0, doTestTypedArrayBigEndian),
]);

new BenchmarkSuite('DataViewTest-TypedArray-LittleEndian', [1000], [
  new Benchmark('DataViewTest-TypedArray-LittleEndian', false, false, 0, doTestTypedArrayLittleEndian),
]);

new BenchmarkSuite('DataViewTest-TypedArray-Floats', [1000], [
  new Benchmark('DataViewTest-TypedArray-Floats', false, false, 0, doTestTypedArrayFloats),
]);

function doTestDataViewBigEndian() {
  doIterations(false, true);
}

function doTestDataViewLittleEndian() {
  doIterations(true, true);
}

function doTestTypedArrayBigEndian() {
  doIterations(false, false);
}

function doTestTypedArrayLittleEndian() {
  doIterations(true, false);
}

function doTestDataViewFloats() {
  doFloatIterations(true);
}

function doTestTypedArrayFloats() {
  doFloatIterations(false);
}

function doIterations(littleEndian, dataView) {
  var buffer = makeBuffer(1000, littleEndian);
  var iterations = 10;
  if (dataView) {
    for (var i = 0; i < iterations; i++)
      doOneIterationDV(buffer, littleEndian);
  } else {
    for (var i = 0; i < iterations; i++)
      doOneIterationJS(buffer, littleEndian);
  }
}

function makeBuffer(size, littleEndian) {
  var buffer = new ArrayBuffer(size * 14);
  var view = new DataView(buffer);
  for (var i = 0; i < size; ++i) {
    view.setInt8(i * 14, i);
    view.setUint8(i * 14 + 1, i);
    view.setInt16(i * 14 + 2, i * i, littleEndian);
    view.setUint16(i * 14 + 4, i * i, littleEndian);
    view.setInt32(i * 14 + 6, i * i * i, littleEndian);
    view.setUint32(i * 14 + 10, i * i * i, littleEndian);
  }
  return buffer;
}

function doOneIterationDV(buffer, littleEndian) {
  var xor = 0;
  var view = new DataView(buffer);
  for (var i = 0; i < view.byteLength; i += 14) {
    xor ^= view.getInt8(i);
    xor ^= view.getUint8(i + 1);
    xor ^= view.getInt16(i + 2, littleEndian);
    xor ^= view.getUint16(i + 4, littleEndian);
    xor ^= view.getInt32(i + 6, littleEndian);
    xor ^= view.getUint32(i + 10, littleEndian);
  }
}

function doOneIterationJS(buffer, littleEndian) {
  var xor = 0;
  var reader;
  if (littleEndian) {
    reader = new LittleEndian(buffer);
  } else {
    reader = new BigEndian(buffer);
  }
  for (var i = 0; i < buffer.byteLength; i += 14) {
    xor ^= reader.getInt8(i);
    xor ^= reader.getUint8(i + 1);
    xor ^= reader.getInt16(i + 2);
    xor ^= reader.getUint16(i + 4);
    xor ^= reader.getInt32(i + 6);
    xor ^= reader.getUint32(i + 10);
  }
}

function doFloatIterations(dataView) {
  var buffer = makeFloatBuffer(1000);
  var iterations = 10;
  if (dataView) {
    for (var i = 0; i < iterations; i++)
      doOneFloatIterationDV(buffer);
  } else {
    for (var i = 0; i < iterations; i++)
      doOneFloatIterationJS(buffer);
  }
}

function makeFloatBuffer(size) {
  var buffer = new ArrayBuffer(size * 16);
  var view = new DataView(buffer);
  for (var i = 0; i < size; i++) {
    view.setFloat64(i * 16, Math.log10(i + 1));
    view.setFloat32(i * 16 + 8, Math.sqrt(i));
    view.setFloat32(i * 16 + 12, Math.cos(i));
  }
  return buffer;
}

function doOneFloatIterationDV(buffer) {
  var sum = 0;
  var view = new DataView(buffer);
  for (var i = 0; i < view.byteLength; i += 16) {
    sum += view.getFloat64(i);
    sum += view.getFloat32(i + 8);
    sum += view.getFloat32(i + 12);
  }
}

function doOneFloatIterationJS(buffer) {
  var sum = 0;
  var float32array = new Float32Array(buffer);
  var float64array = new Float64Array(buffer);
  for (var i = 0; i < buffer.byteLength; i += 16) {
    sum += float64array[i/8];
    sum += float32array[i/4 + 2];
    sum += float32array[i/4 + 3];
  }
}

function BigEndian(buffer, opt_byteOffset) {
  this.uint8View_ = new Uint8Array(buffer, opt_byteOffset || 0);
  this.int8View_ = new Int8Array(buffer, opt_byteOffset || 0);
}
BigEndian.prototype.getInt8 = function(byteOffset) {
  return this.int8View_[byteOffset];
};
BigEndian.prototype.getUint8 = function(byteOffset) {
  return this.uint8View_[byteOffset];
};
BigEndian.prototype.getInt16 = function(byteOffset) {
  return this.uint8View_[byteOffset + 1] | (this.int8View_[byteOffset] << 8);
};
BigEndian.prototype.getUint16 = function(byteOffset) {
  return this.uint8View_[byteOffset + 1] | (this.uint8View_[byteOffset] << 8);
};
BigEndian.prototype.getInt32 = function(byteOffset) {
  return this.uint8View_[byteOffset + 3] |
      (this.uint8View_[byteOffset + 2] << 8) |
      (this.uint8View_[byteOffset + 1] << 16) |
      (this.int8View_[byteOffset] << 24);
};
BigEndian.prototype.getUint32 = function(byteOffset) {
  return this.uint8View_[byteOffset + 3] +
      (this.uint8View_[byteOffset + 2] << 8) +
      (this.uint8View_[byteOffset + 1] << 16) +
      (this.uint8View_[byteOffset] * (1 << 24));
};

function LittleEndian(buffer, opt_byteOffset) {
  this.uint8View_ = new Uint8Array(buffer, opt_byteOffset || 0);
  this.int8View_ = new Int8Array(buffer, opt_byteOffset || 0);
}
LittleEndian.prototype.getInt8 = function(byteOffset) {
  return this.int8View_[byteOffset];
};
LittleEndian.prototype.getUint8 = function(byteOffset) {
  return this.uint8View_[byteOffset];
};
LittleEndian.prototype.getInt16 = function(byteOffset) {
  return this.uint8View_[byteOffset] | (this.int8View_[byteOffset + 1] << 8);
};
LittleEndian.prototype.getUint16 = function(byteOffset) {
  return this.uint8View_[byteOffset] | (this.uint8View_[byteOffset + 1] << 8);
};
LittleEndian.prototype.getInt32 = function(byteOffset) {
  return this.uint8View_[byteOffset] |
      (this.uint8View_[byteOffset + 1] << 8) |
      (this.uint8View_[byteOffset + 2] << 16) |
      (this.int8View_[byteOffset + 3] << 24);
};
LittleEndian.prototype.getUint32 = function(byteOffset) {
  return this.uint8View_[byteOffset] +
      (this.uint8View_[byteOffset + 1] << 8) +
      (this.uint8View_[byteOffset + 2] << 16) +
      (this.uint8View_[byteOffset + 3] * (1 << 24));
};
