// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function(global, utils, extrasUtils) {

"use strict";

%CheckIsBootstrapping();

var GlobalArray = global.Array;
// It is important that this file is run after src/js/typedarray.js,
// otherwise GlobalTypedArray would be Object, and we would break
// old versions of Zepto.
var GlobalTypedArray = global.Uint8Array.__proto__;
var GlobalMap = global.Map;
var GlobalSet = global.Set;
var GlobalArrayBuffer = global.ArrayBuffer;
var GlobalPromise = global.Promise;
var GlobalRegExp = global.RegExp;
var speciesSymbol = utils.ImportNow("species_symbol");

function ArraySpecies() {
  return this;
}

function TypedArraySpecies() {
  return this;
}

function MapSpecies() {
  return this;
}

function SetSpecies() {
  return this;
}

function ArrayBufferSpecies() {
  return this;
}

function PromiseSpecies() {
  return this;
}

function RegExpSpecies() {
  return this;
}

utils.InstallGetter(GlobalArray, speciesSymbol, ArraySpecies, DONT_ENUM);
utils.InstallGetter(GlobalTypedArray, speciesSymbol, TypedArraySpecies, DONT_ENUM);
utils.InstallGetter(GlobalMap, speciesSymbol, MapSpecies, DONT_ENUM);
utils.InstallGetter(GlobalSet, speciesSymbol, SetSpecies, DONT_ENUM);
utils.InstallGetter(GlobalArrayBuffer, speciesSymbol, ArrayBufferSpecies,
                    DONT_ENUM);
utils.InstallGetter(GlobalPromise, speciesSymbol, PromiseSpecies, DONT_ENUM);
utils.InstallGetter(GlobalRegExp, speciesSymbol, RegExpSpecies, DONT_ENUM);

});
