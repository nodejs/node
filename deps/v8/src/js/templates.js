// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Called from a desugaring in the parser.

(function(global, utils) {

"use strict";

%CheckIsBootstrapping();

// -------------------------------------------------------------------
// Imports

var GlobalMap = global.Map;
var InternalArray = utils.InternalArray;

// -------------------------------------------------------------------

var callSiteCache = new GlobalMap;
var mapGetFn = GlobalMap.prototype.get;
var mapSetFn = GlobalMap.prototype.set;


function SameCallSiteElements(rawStrings, other) {
  var length = rawStrings.length;
  var other = other.raw;

  if (length !== other.length) return false;

  for (var i = 0; i < length; ++i) {
    if (rawStrings[i] !== other[i]) return false;
  }

  return true;
}


function GetCachedCallSite(siteObj, hash) {
  var obj = %_Call(mapGetFn, callSiteCache, hash);

  if (IS_UNDEFINED(obj)) return;

  var length = obj.length;
  for (var i = 0; i < length; ++i) {
    if (SameCallSiteElements(siteObj, obj[i])) return obj[i];
  }
}


function SetCachedCallSite(siteObj, hash) {
  var obj = %_Call(mapGetFn, callSiteCache, hash);
  var array;

  if (IS_UNDEFINED(obj)) {
    array = new InternalArray(1);
    array[0] = siteObj;
    %_Call(mapSetFn, callSiteCache, hash, array);
  } else {
    obj.push(siteObj);
  }

  return siteObj;
}


function GetTemplateCallSite(siteObj, rawStrings, hash) {
  var cached = GetCachedCallSite(rawStrings, hash);

  if (!IS_UNDEFINED(cached)) return cached;

  %AddNamedProperty(siteObj, "raw", %object_freeze(rawStrings),
      READ_ONLY | DONT_ENUM | DONT_DELETE);

  return SetCachedCallSite(%object_freeze(siteObj), hash);
}

// ----------------------------------------------------------------------------
// Exports

%InstallToContext(["get_template_call_site", GetTemplateCallSite]);

})
