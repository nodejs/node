// Copyright 2006-2008 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This files contains runtime support implemented in JavaScript.

// CAUTION: Some of the functions specified in this file are called
// directly from compiled code. These are the functions with names in
// ALL CAPS. The compiled code passes the first argument in 'this'.


// The following declarations are shared with other native JS files.
// They are all declared at this one spot to avoid redeclaration errors.

(function(global, utils) {

%CheckIsBootstrapping();

var FLAG_harmony_species;
var GlobalArray = global.Array;
var GlobalBoolean = global.Boolean;
var GlobalString = global.String;
var MakeRangeError;
var MakeTypeError;
var speciesSymbol;

utils.Import(function(from) {
  MakeRangeError = from.MakeRangeError;
  MakeTypeError = from.MakeTypeError;
  speciesSymbol = from.species_symbol;
});

utils.ImportFromExperimental(function(from) {
  FLAG_harmony_species = from.FLAG_harmony_species;
});

// ----------------------------------------------------------------------------


/* ---------------------------------
   - - -   U t i l i t i e s   - - -
   ---------------------------------
*/

function ConcatIterableToArray(target, iterable) {
   var index = target.length;
   for (var element of iterable) {
     AddIndexedProperty(target, index++, element);
   }
   return target;
}


// This function should be called rather than %AddElement in contexts where the
// argument might not be less than 2**32-1. ES2015 ToLength semantics mean that
// this is a concern at basically all callsites.
function AddIndexedProperty(obj, index, value) {
  if (index === TO_UINT32(index) && index !== kMaxUint32) {
    %AddElement(obj, index, value);
  } else {
    %AddNamedProperty(obj, TO_STRING(index), value, NONE);
  }
}
%SetForceInlineFlag(AddIndexedProperty);


function ToPositiveInteger(x, rangeErrorIndex) {
  var i = TO_INTEGER_MAP_MINUS_ZERO(x);
  if (i < 0) throw MakeRangeError(rangeErrorIndex);
  return i;
}


function MaxSimple(a, b) {
  return a > b ? a : b;
}


function MinSimple(a, b) {
  return a > b ? b : a;
}


%SetForceInlineFlag(MaxSimple);
%SetForceInlineFlag(MinSimple);


// ES2015 7.3.20
// For the fallback with --harmony-species off, there are two possible choices:
//  - "conservative": return defaultConstructor
//  - "not conservative": return object.constructor
// This fallback path is only needed in the transition to ES2015, and the
// choice is made simply to preserve the previous behavior so that we don't
// have a three-step upgrade: old behavior, unspecified intermediate behavior,
// and ES2015.
// In some cases, we were "conservative" (e.g., ArrayBuffer, RegExp), and in
// other cases we were "not conservative (e.g., TypedArray, Promise).
function SpeciesConstructor(object, defaultConstructor, conservative) {
  if (FLAG_harmony_species) {
    var constructor = object.constructor;
    if (IS_UNDEFINED(constructor)) {
      return defaultConstructor;
    }
    if (!IS_RECEIVER(constructor)) {
      throw MakeTypeError(kConstructorNotReceiver);
    }
    var species = constructor[speciesSymbol];
    if (IS_NULL_OR_UNDEFINED(species)) {
      return defaultConstructor;
    }
    if (%IsConstructor(species)) {
      return species;
    }
    throw MakeTypeError(kSpeciesNotConstructor);
  } else {
    return conservative ? defaultConstructor : object.constructor;
  }
}

//----------------------------------------------------------------------------

// NOTE: Setting the prototype for Array must take place as early as
// possible due to code generation for array literals.  When
// generating code for a array literal a boilerplate array is created
// that is cloned when running the code.  It is essential that the
// boilerplate gets the right prototype.
%FunctionSetPrototype(GlobalArray, new GlobalArray(0));

// ----------------------------------------------------------------------------
// Exports

utils.Export(function(to) {
  to.AddIndexedProperty = AddIndexedProperty;
  to.MaxSimple = MaxSimple;
  to.MinSimple = MinSimple;
  to.ToPositiveInteger = ToPositiveInteger;
  to.SpeciesConstructor = SpeciesConstructor;
});

%InstallToContext([
  "concat_iterable_to_array", ConcatIterableToArray,
]);

})
