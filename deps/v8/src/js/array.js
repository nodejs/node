// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function(global, utils, extrasUtils) {

"use strict";

%CheckIsBootstrapping();

// -------------------------------------------------------------------
// Imports

var GlobalArray = global.Array;
var InternalArray = utils.InternalArray;
var ObjectToString = global.Object.prototype.toString;
var iteratorSymbol = utils.ImportNow("iterator_symbol");
var unscopablesSymbol = utils.ImportNow("unscopables_symbol");

// -------------------------------------------------------------------

macro INVERT_NEG_ZERO(arg)
((arg) + 0)
endmacro

function ArraySpeciesCreate(array, length) {
  length = INVERT_NEG_ZERO(length);
  var constructor = %ArraySpeciesConstructor(array);
  return new constructor(length);
}


function KeySortCompare(a, b) {
  return a - b;
}

function GetSortedArrayKeys(array, indices) {
  if (IS_NUMBER(indices)) {
    // It's an interval
    var limit = indices;
    var keys = new InternalArray();
    for (var i = 0; i < limit; ++i) {
      var e = array[i];
      if (!IS_UNDEFINED(e) || i in array) {
        keys.push(i);
      }
    }
    return keys;
  }
  return InnerArraySort(indices, indices.length, KeySortCompare);
}


function SparseJoinWithSeparatorJS(
    array, keys, length, use_locale, separator, locales, options) {
  var keys_length = keys.length;
  var elements = new InternalArray(keys_length * 2);
  for (var i = 0; i < keys_length; i++) {
    var key = keys[i];
    elements[i * 2] = key;
    elements[i * 2 + 1] = ConvertToString(
        use_locale, array[key], locales, options);
  }
  return %SparseJoinWithSeparator(elements, length, separator);
}


// Optimized for sparse arrays if separator is ''.
function SparseJoin(array, keys, use_locale, locales, options) {
  var keys_length = keys.length;
  var elements = new InternalArray(keys_length);
  for (var i = 0; i < keys_length; i++) {
    elements[i] = ConvertToString(use_locale, array[keys[i]], locales, options);
  }
  return %StringBuilderConcat(elements, keys_length, '');
}


function UseSparseVariant(array, length, is_array, touched) {
  // Only use the sparse variant on arrays that are likely to be sparse and the
  // number of elements touched in the operation is relatively small compared to
  // the overall size of the array.
  if (!is_array || length < 1000 || %HasComplexElements(array)) {
    return false;
  }
  if (!%_IsSmi(length)) {
    return true;
  }
  var elements_threshold = length >> 2;  // No more than 75% holes
  var estimated_elements = %EstimateNumberOfElements(array);
  return (estimated_elements < elements_threshold) &&
    (touched > estimated_elements * 4);
}

function Stack() {
  this.length = 0;
  this.values = new InternalArray();
}

// Predeclare the instance variables on the prototype. Otherwise setting them in
// the constructor will leak the instance through settings on Object.prototype.
Stack.prototype.length = null;
Stack.prototype.values = null;

function StackPush(stack, value) {
  stack.values[stack.length++] = value;
}

function StackPop(stack) {
  stack.values[--stack.length] = null
}

function StackHas(stack, v) {
  var length = stack.length;
  var values = stack.values;
  for (var i = 0; i < length; i++) {
    if (values[i] === v) return true;
  }
  return false;
}

// Global list of arrays visited during toString, toLocaleString and
// join invocations.
var visited_arrays = new Stack();

function DoJoin(
    array, length, is_array, separator, use_locale, locales, options) {
  if (UseSparseVariant(array, length, is_array, length)) {
    %NormalizeElements(array);
    var keys = GetSortedArrayKeys(array, %GetArrayKeys(array, length));
    if (separator === '') {
      if (keys.length === 0) return '';
      return SparseJoin(array, keys, use_locale, locales, options);
    } else {
      return SparseJoinWithSeparatorJS(
          array, keys, length, use_locale, separator, locales, options);
    }
  }

  // Fast case for one-element arrays.
  if (length === 1) {
    return ConvertToString(use_locale, array[0], locales, options);
  }

  // Construct an array for the elements.
  var elements = new InternalArray(length);
  for (var i = 0; i < length; i++) {
    elements[i] = ConvertToString(use_locale, array[i], locales, options);
  }

  if (separator === '') {
    return %StringBuilderConcat(elements, length, '');
  } else {
    return %StringBuilderJoin(elements, length, separator);
  }
}

function Join(array, length, separator, use_locale, locales, options) {
  if (length === 0) return '';

  var is_array = IS_ARRAY(array);

  if (is_array) {
    // If the array is cyclic, return the empty string for already
    // visited arrays.
    if (StackHas(visited_arrays, array)) return '';
    StackPush(visited_arrays, array);
  }

  // Attempt to convert the elements.
  try {
    return DoJoin(
        array, length, is_array, separator, use_locale, locales, options);
  } finally {
    // Make sure to remove the last element of the visited array no
    // matter what happens.
    if (is_array) StackPop(visited_arrays);
  }
}


function ConvertToString(use_locale, x, locales, options) {
  if (IS_NULL_OR_UNDEFINED(x)) return '';
  if (use_locale) {
    if (IS_NULL_OR_UNDEFINED(locales)) {
      return TO_STRING(x.toLocaleString());
    } else if (IS_NULL_OR_UNDEFINED(options)) {
      return TO_STRING(x.toLocaleString(locales));
    }
    return TO_STRING(x.toLocaleString(locales, options));
  }

  return TO_STRING(x);
}


// -------------------------------------------------------------------

var ArrayJoin;
DEFINE_METHOD(
  GlobalArray.prototype,
  toString() {
    var array;
    var func;
    if (IS_ARRAY(this)) {
      func = this.join;
      if (func === ArrayJoin) {
        return Join(this, this.length, ',', false);
      }
      array = this;
    } else {
      array = TO_OBJECT(this);
      func = array.join;
    }
    if (!IS_CALLABLE(func)) {
      return %_Call(ObjectToString, array);
    }
    return %_Call(func, array);
  }
);

// ecma402 #sup-array.prototype.tolocalestring
function InnerArrayToLocaleString(array, length, locales, options) {
  return Join(array, TO_LENGTH(length), ',', true, locales, options);
}


DEFINE_METHOD(
  GlobalArray.prototype,
  // ecma402 #sup-array.prototype.tolocalestring
  toLocaleString() {
    var array = TO_OBJECT(this);
    var arrayLen = array.length;
    var locales = arguments[0];
    var options = arguments[1];
    return InnerArrayToLocaleString(array, arrayLen, locales, options);
  }
);


function InnerArrayJoin(separator, array, length) {
  if (IS_UNDEFINED(separator)) {
    separator = ',';
  } else {
    separator = TO_STRING(separator);
  }

  // Fast case for one-element arrays.
  if (length === 1) {
    var e = array[0];
    if (IS_NULL_OR_UNDEFINED(e)) return '';
    return TO_STRING(e);
  }

  return Join(array, length, separator, false);
}


DEFINE_METHOD(
  GlobalArray.prototype,
  join(separator) {
    var array = TO_OBJECT(this);
    var length = TO_LENGTH(array.length);

    return InnerArrayJoin(separator, array, length);
  }
);


// Oh the humanity... don't remove the following function because js2c for some
// reason gets symbol minifiation wrong if it's not there. Instead of spending
// the time fixing js2c (which will go away when all of the internal .js runtime
// files are gone), just keep this work-around.
function ArraySliceFallback(start, end) {
  return null;
}

function InnerArraySort(array, length, comparefn) {
  // In-place QuickSort algorithm.
  // For short (length <= 10) arrays, insertion sort is used for efficiency.

  if (!IS_CALLABLE(comparefn)) {
    comparefn = function (x, y) {
      if (x === y) return 0;
      if (%_IsSmi(x) && %_IsSmi(y)) {
        return %SmiLexicographicCompare(x, y);
      }
      x = TO_STRING(x);
      y = TO_STRING(y);
      if (x == y) return 0;
      else return x < y ? -1 : 1;
    };
  }
  function InsertionSort(a, from, to) {
    for (var i = from + 1; i < to; i++) {
      var element = a[i];
      for (var j = i - 1; j >= from; j--) {
        var tmp = a[j];
        var order = comparefn(tmp, element);
        if (order > 0) {
          a[j + 1] = tmp;
        } else {
          break;
        }
      }
      a[j + 1] = element;
    }
  };

  function GetThirdIndex(a, from, to) {
    var t_array = new InternalArray();
    // Use both 'from' and 'to' to determine the pivot candidates.
    var increment = 200 + ((to - from) & 15);
    var j = 0;
    from += 1;
    to -= 1;
    for (var i = from; i < to; i += increment) {
      t_array[j] = [i, a[i]];
      j++;
    }
    t_array.sort(function(a, b) {
      return comparefn(a[1], b[1]);
    });
    var third_index = t_array[t_array.length >> 1][0];
    return third_index;
  }

  function QuickSort(a, from, to) {
    var third_index = 0;
    while (true) {
      // Insertion sort is faster for short arrays.
      if (to - from <= 10) {
        InsertionSort(a, from, to);
        return;
      }
      if (to - from > 1000) {
        third_index = GetThirdIndex(a, from, to);
      } else {
        third_index = from + ((to - from) >> 1);
      }
      // Find a pivot as the median of first, last and middle element.
      var v0 = a[from];
      var v1 = a[to - 1];
      var v2 = a[third_index];
      var c01 = comparefn(v0, v1);
      if (c01 > 0) {
        // v1 < v0, so swap them.
        var tmp = v0;
        v0 = v1;
        v1 = tmp;
      } // v0 <= v1.
      var c02 = comparefn(v0, v2);
      if (c02 >= 0) {
        // v2 <= v0 <= v1.
        var tmp = v0;
        v0 = v2;
        v2 = v1;
        v1 = tmp;
      } else {
        // v0 <= v1 && v0 < v2
        var c12 = comparefn(v1, v2);
        if (c12 > 0) {
          // v0 <= v2 < v1
          var tmp = v1;
          v1 = v2;
          v2 = tmp;
        }
      }
      // v0 <= v1 <= v2
      a[from] = v0;
      a[to - 1] = v2;
      var pivot = v1;
      var low_end = from + 1;   // Upper bound of elements lower than pivot.
      var high_start = to - 1;  // Lower bound of elements greater than pivot.
      a[third_index] = a[low_end];
      a[low_end] = pivot;

      // From low_end to i are elements equal to pivot.
      // From i to high_start are elements that haven't been compared yet.
      partition: for (var i = low_end + 1; i < high_start; i++) {
        var element = a[i];
        var order = comparefn(element, pivot);
        if (order < 0) {
          a[i] = a[low_end];
          a[low_end] = element;
          low_end++;
        } else if (order > 0) {
          do {
            high_start--;
            if (high_start == i) break partition;
            var top_elem = a[high_start];
            order = comparefn(top_elem, pivot);
          } while (order > 0);
          a[i] = a[high_start];
          a[high_start] = element;
          if (order < 0) {
            element = a[i];
            a[i] = a[low_end];
            a[low_end] = element;
            low_end++;
          }
        }
      }
      if (to - high_start < low_end - from) {
        QuickSort(a, high_start, to);
        to = low_end;
      } else {
        QuickSort(a, from, low_end);
        from = high_start;
      }
    }
  };

  if (length < 2) return array;

  // For compatibility with JSC, we also sort elements inherited from
  // the prototype chain on non-Array objects.
  // We do this by copying them to this object and sorting only
  // own elements. This is not very efficient, but sorting with
  // inherited elements happens very, very rarely, if at all.
  // The specification allows "implementation dependent" behavior
  // if an element on the prototype chain has an element that
  // might interact with sorting.
  //
  // We also move all non-undefined elements to the front of the
  // array and move the undefineds after that. Holes are removed.
  // This happens for Array as well as non-Array objects.
  var num_non_undefined = %PrepareElementsForSort(array, length);

  QuickSort(array, 0, num_non_undefined);

  return array;
}


// Set up unscopable properties on the Array.prototype object.
var unscopables = {
  __proto__: null,
  copyWithin: true,
  entries: true,
  fill: true,
  find: true,
  findIndex: true,
  includes: true,
  keys: true,
};

%ToFastProperties(unscopables);

%AddNamedProperty(GlobalArray.prototype, unscopablesSymbol, unscopables,
                  DONT_ENUM | READ_ONLY);

var ArrayIndexOf = GlobalArray.prototype.indexOf;
var ArrayJoin = GlobalArray.prototype.join;
var ArrayPop = GlobalArray.prototype.pop;
var ArrayPush = GlobalArray.prototype.push;
var ArraySlice = GlobalArray.prototype.slice;
var ArrayShift = GlobalArray.prototype.shift;
var ArraySort = GlobalArray.prototype.sort;
var ArraySplice = GlobalArray.prototype.splice;
var ArrayToString = GlobalArray.prototype.toString;
var ArrayUnshift = GlobalArray.prototype.unshift;

// Array prototype functions that return iterators. They are exposed to the
// public API via Template::SetIntrinsicDataProperty().
var ArrayEntries = GlobalArray.prototype.entries;
var ArrayForEach = GlobalArray.prototype.forEach;
var ArrayKeys = GlobalArray.prototype.keys;
var ArrayValues = GlobalArray.prototype[iteratorSymbol];


// The internal Array prototype doesn't need to be fancy, since it's never
// exposed to user code.
// Adding only the functions that are actually used.
utils.SetUpLockedPrototype(InternalArray, GlobalArray(), [
  "indexOf", ArrayIndexOf,
  "join", ArrayJoin,
  "pop", ArrayPop,
  "push", ArrayPush,
  "shift", ArrayShift,
  "sort", ArraySort,
  "splice", ArraySplice
]);

// V8 extras get a separate copy of InternalPackedArray. We give them the basic
// manipulation methods.
utils.SetUpLockedPrototype(extrasUtils.InternalPackedArray, GlobalArray(), [
  "push", ArrayPush,
  "pop", ArrayPop,
  "shift", ArrayShift,
  "unshift", ArrayUnshift,
  "splice", ArraySplice,
  "slice", ArraySlice
]);

// -------------------------------------------------------------------
// Exports

utils.Export(function(to) {
  to.ArrayJoin = ArrayJoin;
  to.ArrayPush = ArrayPush;
  to.ArrayToString = ArrayToString;
  to.ArrayValues = ArrayValues;
  to.InnerArrayJoin = InnerArrayJoin;
  to.InnerArrayToLocaleString = InnerArrayToLocaleString;
});

%InstallToContext([
  "array_entries_iterator", ArrayEntries,
  "array_for_each_iterator", ArrayForEach,
  "array_keys_iterator", ArrayKeys,
  "array_values_iterator", ArrayValues,
]);

});
