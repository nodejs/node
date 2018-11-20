// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"use strict";

var callSiteCache = new $Map;

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
  var obj = %MapGet(callSiteCache, hash);

  if (IS_UNDEFINED(obj)) return;

  var length = obj.length;
  for (var i = 0; i < length; ++i) {
    if (SameCallSiteElements(siteObj, obj[i])) return obj[i];
  }
}


function SetCachedCallSite(siteObj, hash) {
  var obj = %MapGet(callSiteCache, hash);
  var array;

  if (IS_UNDEFINED(obj)) {
    array = new InternalArray(1);
    array[0] = siteObj;
    %MapSet(callSiteCache, hash, array);
  } else {
    obj.push(siteObj);
  }

  return siteObj;
}


function GetTemplateCallSite(siteObj, rawStrings, hash) {
  var cached = GetCachedCallSite(rawStrings, hash);

  if (!IS_UNDEFINED(cached)) return cached;

  %AddNamedProperty(siteObj, "raw", %ObjectFreeze(rawStrings),
      READ_ONLY | DONT_ENUM | DONT_DELETE);

  return SetCachedCallSite(%ObjectFreeze(siteObj), hash);
}


// ES6 Draft 10-14-2014, section 21.1.2.4
function StringRaw(callSite) {
  // TODO(caitp): Use rest parameters when implemented
  var numberOfSubstitutions = %_ArgumentsLength();
  var cooked = ToObject(callSite);
  var raw = ToObject(cooked.raw);
  var literalSegments = ToLength(raw.length);
  if (literalSegments <= 0) return "";

  var result = ToString(raw[0]);

  for (var i = 1; i < literalSegments; ++i) {
    if (i < numberOfSubstitutions) {
      result += ToString(%_Arguments(i));
    }
    result += ToString(raw[i]);
  }

  return result;
}


function ExtendStringForTemplates() {
  %CheckIsBootstrapping();

  // Set up the non-enumerable functions on the String object.
  InstallFunctions($String, DONT_ENUM, $Array(
    "raw", StringRaw
  ));
}

ExtendStringForTemplates();
