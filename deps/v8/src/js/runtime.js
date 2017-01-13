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

var GlobalArray = global.Array;
var GlobalBoolean = global.Boolean;
var GlobalString = global.String;
var speciesSymbol;

utils.Import(function(from) {
  speciesSymbol = from.species_symbol;
});

// ----------------------------------------------------------------------------


/* ---------------------------------
   - - -   U t i l i t i e s   - - -
   ---------------------------------
*/


function ToPositiveInteger(x, rangeErrorIndex) {
  var i = TO_INTEGER(x) + 0;
  if (i < 0) throw %make_range_error(rangeErrorIndex);
  return i;
}


function ToIndex(x, rangeErrorIndex) {
  var i = TO_INTEGER(x) + 0;
  if (i < 0 || i > kMaxSafeInteger) throw %make_range_error(rangeErrorIndex);
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
function SpeciesConstructor(object, defaultConstructor) {
  var constructor = object.constructor;
  if (IS_UNDEFINED(constructor)) {
    return defaultConstructor;
  }
  if (!IS_RECEIVER(constructor)) {
    throw %make_type_error(kConstructorNotReceiver);
  }
  var species = constructor[speciesSymbol];
  if (IS_NULL_OR_UNDEFINED(species)) {
    return defaultConstructor;
  }
  if (%IsConstructor(species)) {
    return species;
  }
  throw %make_type_error(kSpeciesNotConstructor);
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
  to.MaxSimple = MaxSimple;
  to.MinSimple = MinSimple;
  to.ToPositiveInteger = ToPositiveInteger;
  to.ToIndex = ToIndex;
  to.SpeciesConstructor = SpeciesConstructor;
});

})
