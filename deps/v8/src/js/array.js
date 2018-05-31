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
var MathMax = global.Math.max;
var MathMin = global.Math.min;
var ObjectHasOwnProperty = global.Object.prototype.hasOwnProperty;
var ObjectToString = global.Object.prototype.toString;
var iteratorSymbol = utils.ImportNow("iterator_symbol");
var unscopablesSymbol = utils.ImportNow("unscopables_symbol");

// -------------------------------------------------------------------

macro IS_PROXY(arg)
(%_IsJSProxy(arg))
endmacro

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


function SparseJoinWithSeparatorJS(array, keys, length, use_locale, separator) {
  var keys_length = keys.length;
  var elements = new InternalArray(keys_length * 2);
  for (var i = 0; i < keys_length; i++) {
    var key = keys[i];
    elements[i * 2] = key;
    elements[i * 2 + 1] = ConvertToString(use_locale, array[key]);
  }
  return %SparseJoinWithSeparator(elements, length, separator);
}


// Optimized for sparse arrays if separator is ''.
function SparseJoin(array, keys, use_locale) {
  var keys_length = keys.length;
  var elements = new InternalArray(keys_length);
  for (var i = 0; i < keys_length; i++) {
    elements[i] = ConvertToString(use_locale, array[keys[i]]);
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

function DoJoin(array, length, is_array, separator, use_locale) {
  if (UseSparseVariant(array, length, is_array, length)) {
    %NormalizeElements(array);
    var keys = GetSortedArrayKeys(array, %GetArrayKeys(array, length));
    if (separator === '') {
      if (keys.length === 0) return '';
      return SparseJoin(array, keys, use_locale);
    } else {
      return SparseJoinWithSeparatorJS(
          array, keys, length, use_locale, separator);
    }
  }

  // Fast case for one-element arrays.
  if (length === 1) {
    return ConvertToString(use_locale, array[0]);
  }

  // Construct an array for the elements.
  var elements = new InternalArray(length);
  for (var i = 0; i < length; i++) {
    elements[i] = ConvertToString(use_locale, array[i]);
  }

  if (separator === '') {
    return %StringBuilderConcat(elements, length, '');
  } else {
    return %StringBuilderJoin(elements, length, separator);
  }
}

function Join(array, length, separator, use_locale) {
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
    return DoJoin(array, length, is_array, separator, use_locale);
  } finally {
    // Make sure to remove the last element of the visited array no
    // matter what happens.
    if (is_array) StackPop(visited_arrays);
  }
}


function ConvertToString(use_locale, x) {
  if (IS_NULL_OR_UNDEFINED(x)) return '';
  return TO_STRING(use_locale ? x.toLocaleString() : x);
}


// This function implements the optimized splice implementation that can use
// special array operations to handle sparse arrays in a sensible fashion.
function SparseSlice(array, start_i, del_count, len, deleted_elements) {
  // Move deleted elements to a new array (the return value from splice).
  var indices = %GetArrayKeys(array, start_i + del_count);
  if (IS_NUMBER(indices)) {
    var limit = indices;
    for (var i = start_i; i < limit; ++i) {
      var current = array[i];
      if (!IS_UNDEFINED(current) || i in array) {
        %CreateDataProperty(deleted_elements, i - start_i, current);
      }
    }
  } else {
    var length = indices.length;
    for (var k = 0; k < length; ++k) {
      var key = indices[k];
      if (key >= start_i) {
        var current = array[key];
        if (!IS_UNDEFINED(current) || key in array) {
          %CreateDataProperty(deleted_elements, key - start_i, current);
        }
      }
    }
  }
}


// This function implements the optimized splice implementation that can use
// special array operations to handle sparse arrays in a sensible fashion.
function SparseMove(array, start_i, del_count, len, num_additional_args) {
  // Bail out if no moving is necessary.
  if (num_additional_args === del_count) return;
  // Move data to new array.
  var new_array = new InternalArray(
      // Clamp array length to 2^32-1 to avoid early RangeError.
      MathMin(len - del_count + num_additional_args, 0xffffffff));
  var big_indices;
  var indices = %GetArrayKeys(array, len);
  if (IS_NUMBER(indices)) {
    var limit = indices;
    for (var i = 0; i < start_i && i < limit; ++i) {
      var current = array[i];
      if (!IS_UNDEFINED(current) || i in array) {
        new_array[i] = current;
      }
    }
    for (var i = start_i + del_count; i < limit; ++i) {
      var current = array[i];
      if (!IS_UNDEFINED(current) || i in array) {
        new_array[i - del_count + num_additional_args] = current;
      }
    }
  } else {
    var length = indices.length;
    for (var k = 0; k < length; ++k) {
      var key = indices[k];
      if (key < start_i) {
        var current = array[key];
        if (!IS_UNDEFINED(current) || key in array) {
          new_array[key] = current;
        }
      } else if (key >= start_i + del_count) {
        var current = array[key];
        if (!IS_UNDEFINED(current) || key in array) {
          var new_key = key - del_count + num_additional_args;
          new_array[new_key] = current;
          if (new_key > 0xfffffffe) {
            big_indices = big_indices || new InternalArray();
            big_indices.push(new_key);
          }
        }
      }
    }
  }
  // Move contents of new_array into this array
  %MoveArrayContents(new_array, array);
  // Add any moved values that aren't elements anymore.
  if (!IS_UNDEFINED(big_indices)) {
    var length = big_indices.length;
    for (var i = 0; i < length; ++i) {
      var key = big_indices[i];
      array[key] = new_array[key];
    }
  }
}


// This is part of the old simple-minded splice.  We are using it either
// because the receiver is not an array (so we have no choice) or because we
// know we are not deleting or moving a lot of elements.
function SimpleSlice(array, start_i, del_count, len, deleted_elements) {
  for (var i = 0; i < del_count; i++) {
    var index = start_i + i;
    if (index in array) {
      var current = array[index];
      %CreateDataProperty(deleted_elements, i, current);
    }
  }
}


function SimpleMove(array, start_i, del_count, len, num_additional_args) {
  if (num_additional_args !== del_count) {
    // Move the existing elements after the elements to be deleted
    // to the right position in the resulting array.
    if (num_additional_args > del_count) {
      for (var i = len - del_count; i > start_i; i--) {
        var from_index = i + del_count - 1;
        var to_index = i + num_additional_args - 1;
        if (from_index in array) {
          array[to_index] = array[from_index];
        } else {
          delete array[to_index];
        }
      }
    } else {
      for (var i = start_i; i < len - del_count; i++) {
        var from_index = i + del_count;
        var to_index = i + num_additional_args;
        if (from_index in array) {
          array[to_index] = array[from_index];
        } else {
          delete array[to_index];
        }
      }
      for (var i = len; i > len - del_count + num_additional_args; i--) {
        delete array[i - 1];
      }
    }
  }
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

function InnerArrayToLocaleString(array, length) {
  return Join(array, TO_LENGTH(length), ',', true);
}


DEFINE_METHOD(
  GlobalArray.prototype,
  toLocaleString() {
    var array = TO_OBJECT(this);
    var arrayLen = array.length;
    return InnerArrayToLocaleString(array, arrayLen);
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


// Removes the last element from the array and returns it. See
// ECMA-262, section 15.4.4.6.
function ArrayPopFallback() {
  var array = TO_OBJECT(this);
  var n = TO_LENGTH(array.length);
  if (n == 0) {
    array.length = n;
    return;
  }

  n--;
  var value = array[n];
  delete array[n];
  array.length = n;
  return value;
}


// Appends the arguments to the end of the array and returns the new
// length of the array. See ECMA-262, section 15.4.4.7.
function ArrayPushFallback() {
  var array = TO_OBJECT(this);
  var n = TO_LENGTH(array.length);
  var m = arguments.length;

  // Subtract n from kMaxSafeInteger rather than testing m + n >
  // kMaxSafeInteger. n may already be kMaxSafeInteger. In that case adding
  // e.g., 1 would not be safe.
  if (m > kMaxSafeInteger - n) throw %make_type_error(kPushPastSafeLength, m, n);

  for (var i = 0; i < m; i++) {
    array[i+n] = arguments[i];
  }

  var new_length = n + m;
  array.length = new_length;
  return new_length;
}


// For implementing reverse() on large, sparse arrays.
function SparseReverse(array, len) {
  var keys = GetSortedArrayKeys(array, %GetArrayKeys(array, len));
  var high_counter = keys.length - 1;
  var low_counter = 0;
  while (low_counter <= high_counter) {
    var i = keys[low_counter];
    var j = keys[high_counter];

    var j_complement = len - j - 1;
    var low, high;

    if (j_complement <= i) {
      high = j;
      while (keys[--high_counter] == j) { }
      low = j_complement;
    }
    if (j_complement >= i) {
      low = i;
      while (keys[++low_counter] == i) { }
      high = len - i - 1;
    }

    var current_i = array[low];
    if (!IS_UNDEFINED(current_i) || low in array) {
      var current_j = array[high];
      if (!IS_UNDEFINED(current_j) || high in array) {
        array[low] = current_j;
        array[high] = current_i;
      } else {
        array[high] = current_i;
        delete array[low];
      }
    } else {
      var current_j = array[high];
      if (!IS_UNDEFINED(current_j) || high in array) {
        array[low] = current_j;
        delete array[high];
      }
    }
  }
}

function PackedArrayReverse(array, len) {
  var j = len - 1;
  for (var i = 0; i < j; i++, j--) {
    var current_i = array[i];
    var current_j = array[j];
    array[i] = current_j;
    array[j] = current_i;
  }
  return array;
}


function GenericArrayReverse(array, len) {
  var j = len - 1;
  for (var i = 0; i < j; i++, j--) {
    if (i in array) {
      var current_i = array[i];
      if (j in array) {
        var current_j = array[j];
        array[i] = current_j;
        array[j] = current_i;
      } else {
        array[j] = current_i;
        delete array[i];
      }
    } else {
      if (j in array) {
        var current_j = array[j];
        array[i] = current_j;
        delete array[j];
      }
    }
  }
  return array;
}


DEFINE_METHOD(
  GlobalArray.prototype,
  reverse() {
    var array = TO_OBJECT(this);
    var len = TO_LENGTH(array.length);
    var isArray = IS_ARRAY(array);

    if (UseSparseVariant(array, len, isArray, len)) {
      %NormalizeElements(array);
      SparseReverse(array, len);
      return array;
    } else if (isArray && %_HasFastPackedElements(array)) {
      return PackedArrayReverse(array, len);
    } else {
      return GenericArrayReverse(array, len);
    }
  }
);


function ArrayShiftFallback() {
  var array = TO_OBJECT(this);
  var len = TO_LENGTH(array.length);

  if (len === 0) {
    array.length = 0;
    return;
  }

  if (%object_is_sealed(array)) throw %make_type_error(kArrayFunctionsOnSealed);

  var first = array[0];

  if (UseSparseVariant(array, len, IS_ARRAY(array), len)) {
    SparseMove(array, 0, 1, len, 0);
  } else {
    SimpleMove(array, 0, 1, len, 0);
  }

  array.length = len - 1;

  return first;
}


function ArrayUnshiftFallback(arg1) {  // length == 1
  var array = TO_OBJECT(this);
  var len = TO_LENGTH(array.length);
  var num_arguments = arguments.length;

  if (len > 0 && UseSparseVariant(array, len, IS_ARRAY(array), len) &&
      !%object_is_sealed(array)) {
    SparseMove(array, 0, 0, len, num_arguments);
  } else {
    SimpleMove(array, 0, 0, len, num_arguments);
  }

  for (var i = 0; i < num_arguments; i++) {
    array[i] = arguments[i];
  }

  var new_length = len + num_arguments;
  array.length = new_length;
  return new_length;
}


// Oh the humanity... don't remove the following function because js2c for some
// reason gets symbol minifiation wrong if it's not there. Instead of spending
// the time fixing js2c (which will go away when all of the internal .js runtime
// files are gone), just keep this work-around.
function ArraySliceFallback(start, end) {
  return null;
}

function ComputeSpliceStartIndex(start_i, len) {
  if (start_i < 0) {
    start_i += len;
    return start_i < 0 ? 0 : start_i;
  }

  return start_i > len ? len : start_i;
}


function ComputeSpliceDeleteCount(delete_count, num_arguments, len, start_i) {
  // SpiderMonkey, TraceMonkey and JSC treat the case where no delete count is
  // given as a request to delete all the elements from the start.
  // And it differs from the case of undefined delete count.
  // This does not follow ECMA-262, but we do the same for
  // compatibility.
  var del_count = 0;
  if (num_arguments == 1)
    return len - start_i;

  del_count = TO_INTEGER(delete_count);
  if (del_count < 0)
    return 0;

  if (del_count > len - start_i)
    return len - start_i;

  return del_count;
}


function ArraySpliceFallback(start, delete_count) {
  var num_arguments = arguments.length;
  var array = TO_OBJECT(this);
  var len = TO_LENGTH(array.length);
  var start_i = ComputeSpliceStartIndex(TO_INTEGER(start), len);
  var del_count = ComputeSpliceDeleteCount(delete_count, num_arguments, len,
                                           start_i);
  var deleted_elements = ArraySpeciesCreate(array, del_count);
  deleted_elements.length = del_count;
  var num_elements_to_add = num_arguments > 2 ? num_arguments - 2 : 0;

  if (del_count != num_elements_to_add && %object_is_sealed(array)) {
    throw %make_type_error(kArrayFunctionsOnSealed);
  } else if (del_count > 0 && %object_is_frozen(array)) {
    throw %make_type_error(kArrayFunctionsOnFrozen);
  }

  var changed_elements = del_count;
  if (num_elements_to_add != del_count) {
    // If the slice needs to do a actually move elements after the insertion
    // point, then include those in the estimate of changed elements.
    changed_elements += len - start_i - del_count;
  }
  if (UseSparseVariant(array, len, IS_ARRAY(array), changed_elements)) {
    %NormalizeElements(array);
    if (IS_ARRAY(deleted_elements)) %NormalizeElements(deleted_elements);
    SparseSlice(array, start_i, del_count, len, deleted_elements);
    SparseMove(array, start_i, del_count, len, num_elements_to_add);
  } else {
    SimpleSlice(array, start_i, del_count, len, deleted_elements);
    SimpleMove(array, start_i, del_count, len, num_elements_to_add);
  }

  // Insert the arguments into the resulting array in
  // place of the deleted elements.
  var i = start_i;
  var arguments_index = 2;
  var arguments_length = arguments.length;
  while (arguments_index < arguments_length) {
    array[i++] = arguments[arguments_index++];
  }
  array.length = len - del_count + num_elements_to_add;

  // Return the deleted elements.
  return deleted_elements;
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

  // Copy elements in the range 0..length from obj's prototype chain
  // to obj itself, if obj has holes. Return one more than the maximal index
  // of a prototype property.
  function CopyFromPrototype(obj, length) {
    var max = 0;
    for (var proto = %object_get_prototype_of(obj); proto;
         proto = %object_get_prototype_of(proto)) {
      var indices = IS_PROXY(proto) ? length : %GetArrayKeys(proto, length);
      if (IS_NUMBER(indices)) {
        // It's an interval.
        var proto_length = indices;
        for (var i = 0; i < proto_length; i++) {
          if (!HAS_OWN_PROPERTY(obj, i) && HAS_OWN_PROPERTY(proto, i)) {
            obj[i] = proto[i];
            if (i >= max) { max = i + 1; }
          }
        }
      } else {
        for (var i = 0; i < indices.length; i++) {
          var index = indices[i];
          if (!HAS_OWN_PROPERTY(obj, index) && HAS_OWN_PROPERTY(proto, index)) {
            obj[index] = proto[index];
            if (index >= max) { max = index + 1; }
          }
        }
      }
    }
    return max;
  };

  // Set a value of "undefined" on all indices in the range from..to
  // where a prototype of obj has an element. I.e., shadow all prototype
  // elements in that range.
  function ShadowPrototypeElements(obj, from, to) {
    for (var proto = %object_get_prototype_of(obj); proto;
         proto = %object_get_prototype_of(proto)) {
      var indices = IS_PROXY(proto) ? to : %GetArrayKeys(proto, to);
      if (IS_NUMBER(indices)) {
        // It's an interval.
        var proto_length = indices;
        for (var i = from; i < proto_length; i++) {
          if (HAS_OWN_PROPERTY(proto, i)) {
            obj[i] = UNDEFINED;
          }
        }
      } else {
        for (var i = 0; i < indices.length; i++) {
          var index = indices[i];
          if (from <= index && HAS_OWN_PROPERTY(proto, index)) {
            obj[index] = UNDEFINED;
          }
        }
      }
    }
  };

  function SafeRemoveArrayHoles(obj) {
    // Copy defined elements from the end to fill in all holes and undefineds
    // in the beginning of the array.  Write undefineds and holes at the end
    // after loop is finished.
    var first_undefined = 0;
    var last_defined = length - 1;
    var num_holes = 0;
    while (first_undefined < last_defined) {
      // Find first undefined element.
      while (first_undefined < last_defined &&
             !IS_UNDEFINED(obj[first_undefined])) {
        first_undefined++;
      }
      // Maintain the invariant num_holes = the number of holes in the original
      // array with indices <= first_undefined or > last_defined.
      if (!HAS_OWN_PROPERTY(obj, first_undefined)) {
        num_holes++;
      }

      // Find last defined element.
      while (first_undefined < last_defined &&
             IS_UNDEFINED(obj[last_defined])) {
        if (!HAS_OWN_PROPERTY(obj, last_defined)) {
          num_holes++;
        }
        last_defined--;
      }
      if (first_undefined < last_defined) {
        // Fill in hole or undefined.
        obj[first_undefined] = obj[last_defined];
        obj[last_defined] = UNDEFINED;
      }
    }
    // If there were any undefineds in the entire array, first_undefined
    // points to one past the last defined element.  Make this true if
    // there were no undefineds, as well, so that first_undefined == number
    // of defined elements.
    if (!IS_UNDEFINED(obj[first_undefined])) first_undefined++;
    // Fill in the undefineds and the holes.  There may be a hole where
    // an undefined should be and vice versa.
    var i;
    for (i = first_undefined; i < length - num_holes; i++) {
      obj[i] = UNDEFINED;
    }
    for (i = length - num_holes; i < length; i++) {
      // For compatibility with Webkit, do not expose elements in the prototype.
      if (i in %object_get_prototype_of(obj)) {
        obj[i] = UNDEFINED;
      } else {
        delete obj[i];
      }
    }

    // Return the number of defined elements.
    return first_undefined;
  };

  if (length < 2) return array;

  var is_array = IS_ARRAY(array);
  var max_prototype_element;
  if (!is_array) {
    // For compatibility with JSC, we also sort elements inherited from
    // the prototype chain on non-Array objects.
    // We do this by copying them to this object and sorting only
    // own elements. This is not very efficient, but sorting with
    // inherited elements happens very, very rarely, if at all.
    // The specification allows "implementation dependent" behavior
    // if an element on the prototype chain has an element that
    // might interact with sorting.
    max_prototype_element = CopyFromPrototype(array, length);
  }

  // %RemoveArrayHoles returns -1 if fast removal is not supported.
  var num_non_undefined = %RemoveArrayHoles(array, length);

  if (num_non_undefined == -1) {
    // There were indexed accessors in the array.
    // Move array holes and undefineds to the end using a Javascript function
    // that is safe in the presence of accessors.
    num_non_undefined = SafeRemoveArrayHoles(array);
  }

  QuickSort(array, 0, num_non_undefined);

  if (!is_array && (num_non_undefined + 1 < max_prototype_element)) {
    // For compatibility with JSC, we shadow any elements in the prototype
    // chain that has become exposed by sort moving a hole to its position.
    ShadowPrototypeElements(array, num_non_undefined, max_prototype_element);
  }

  return array;
}


DEFINE_METHOD(
  GlobalArray.prototype,
  sort(comparefn) {
    if (!IS_UNDEFINED(comparefn) && !IS_CALLABLE(comparefn)) {
      throw %make_type_error(kBadSortComparisonFunction, comparefn);
    }

    var array = TO_OBJECT(this);
    var length = TO_LENGTH(array.length);
    return InnerArraySort(array, length, comparefn);
  }
);

DEFINE_METHOD_LEN(
  GlobalArray.prototype,
  lastIndexOf(element, index) {
    var array = TO_OBJECT(this);
    var length = TO_LENGTH(this.length);

    if (length == 0) return -1;
    if (arguments.length < 2) {
      index = length - 1;
    } else {
      index = INVERT_NEG_ZERO(TO_INTEGER(index));
      // If index is negative, index from end of the array.
      if (index < 0) index += length;
      // If index is still negative, do not search the array.
      if (index < 0) return -1;
      else if (index >= length) index = length - 1;
    }
    var min = 0;
    var max = index;
    if (UseSparseVariant(array, length, IS_ARRAY(array), index)) {
      %NormalizeElements(array);
      var indices = %GetArrayKeys(array, index + 1);
      if (IS_NUMBER(indices)) {
        // It's an interval.
        max = indices;  // Capped by index already.
        // Fall through to loop below.
      } else {
        if (indices.length == 0) return -1;
        // Get all the keys in sorted order.
        var sortedKeys = GetSortedArrayKeys(array, indices);
        var i = sortedKeys.length - 1;
        while (i >= 0) {
          var key = sortedKeys[i];
          if (array[key] === element) return key;
          i--;
        }
        return -1;
      }
    }
    // Lookup through the array.
    if (!IS_UNDEFINED(element)) {
      for (var i = max; i >= min; i--) {
        if (array[i] === element) return i;
      }
      return -1;
    }
    for (var i = max; i >= min; i--) {
      if (IS_UNDEFINED(array[i]) && i in array) {
        return i;
      }
    }
    return -1;
  },
  1  /* Set function length */
);


// ES#sec-array.prototype.copywithin
// (Array.prototype.copyWithin ( target, start [ , end ] )
DEFINE_METHOD_LEN(
  GlobalArray.prototype,
  copyWithin(target, start, end) {
    var array = TO_OBJECT(this);
    var length = TO_LENGTH(array.length);

    target = TO_INTEGER(target);
    var to;
    if (target < 0) {
      to = MathMax(length + target, 0);
    } else {
      to = MathMin(target, length);
    }

    start = TO_INTEGER(start);
    var from;
    if (start < 0) {
      from = MathMax(length + start, 0);
    } else {
      from = MathMin(start, length);
    }

    end = IS_UNDEFINED(end) ? length : TO_INTEGER(end);
    var final;
    if (end < 0) {
      final = MathMax(length + end, 0);
    } else {
      final = MathMin(end, length);
    }

    var count = MathMin(final - from, length - to);
    var direction = 1;
    if (from < to && to < (from + count)) {
      direction = -1;
      from = from + count - 1;
      to = to + count - 1;
    }

    while (count > 0) {
      if (from in array) {
        array[to] = array[from];
      } else {
        delete array[to];
      }
      from = from + direction;
      to = to + direction;
      count--;
    }

    return array;
  },
  2  /* Set function length */
);


// ES6, draft 04-05-14, section 22.1.3.6
DEFINE_METHOD_LEN(
  GlobalArray.prototype,
  fill(value, start, end) {
    var array = TO_OBJECT(this);
    var length = TO_LENGTH(array.length);

    var i = IS_UNDEFINED(start) ? 0 : TO_INTEGER(start);
    var end = IS_UNDEFINED(end) ? length : TO_INTEGER(end);

    if (i < 0) {
      i += length;
      if (i < 0) i = 0;
    } else {
      if (i > length) i = length;
    }

    if (end < 0) {
      end += length;
      if (end < 0) end = 0;
    } else {
      if (end > length) end = length;
    }

    if ((end - i) > 0 && %object_is_frozen(array)) {
      throw %make_type_error(kArrayFunctionsOnFrozen);
    }

    for (; i < end; i++)
      array[i] = value;
    return array;
  },
  1  /* Set function length */
);

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
  to.InnerArraySort = InnerArraySort;
  to.InnerArrayToLocaleString = InnerArrayToLocaleString;
});

%InstallToContext([
  "array_entries_iterator", ArrayEntries,
  "array_for_each_iterator", ArrayForEach,
  "array_keys_iterator", ArrayKeys,
  "array_values_iterator", ArrayValues,
  // Fallback implementations of Array builtins.
  "array_pop", ArrayPopFallback,
  "array_push", ArrayPushFallback,
  "array_shift", ArrayShiftFallback,
  "array_splice", ArraySpliceFallback,
  "array_unshift", ArrayUnshiftFallback,
]);

});
