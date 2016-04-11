// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function(global, utils, extrasUtils) {

"use strict";

%CheckIsBootstrapping();

// -------------------------------------------------------------------
// Imports

var AddIndexedProperty;
var FLAG_harmony_tolength;
var FLAG_harmony_species;
var GetIterator;
var GetMethod;
var GlobalArray = global.Array;
var InternalArray = utils.InternalArray;
var InternalPackedArray = utils.InternalPackedArray;
var MakeTypeError;
var MaxSimple;
var MinSimple;
var ObjectDefineProperty;
var ObjectHasOwnProperty;
var ObjectToString = utils.ImportNow("object_to_string");
var ObserveBeginPerformSplice;
var ObserveEndPerformSplice;
var ObserveEnqueueSpliceRecord;
var SameValueZero;
var iteratorSymbol = utils.ImportNow("iterator_symbol");
var unscopablesSymbol = utils.ImportNow("unscopables_symbol");

utils.Import(function(from) {
  AddIndexedProperty = from.AddIndexedProperty;
  GetIterator = from.GetIterator;
  GetMethod = from.GetMethod;
  MakeTypeError = from.MakeTypeError;
  MaxSimple = from.MaxSimple;
  MinSimple = from.MinSimple;
  ObjectDefineProperty = from.ObjectDefineProperty;
  ObjectHasOwnProperty = from.ObjectHasOwnProperty;
  ObserveBeginPerformSplice = from.ObserveBeginPerformSplice;
  ObserveEndPerformSplice = from.ObserveEndPerformSplice;
  ObserveEnqueueSpliceRecord = from.ObserveEnqueueSpliceRecord;
  SameValueZero = from.SameValueZero;
});

utils.ImportFromExperimental(function(from) {
  FLAG_harmony_tolength = from.FLAG_harmony_tolength;
  FLAG_harmony_species = from.FLAG_harmony_species;
});

// -------------------------------------------------------------------


function ArraySpeciesCreate(array, length) {
  var constructor;
  if (FLAG_harmony_species) {
    constructor = %ArraySpeciesConstructor(array);
  } else {
    constructor = GlobalArray;
  }
  return new constructor(length);
}


function DefineIndexedProperty(array, i, value) {
  if (FLAG_harmony_species) {
    var result = ObjectDefineProperty(array, i, {
      value: value, writable: true, configurable: true, enumerable: true
    });
    if (!result) throw MakeTypeError(kStrictCannotAssign, i);
  } else {
    AddIndexedProperty(array, i, value);
  }
}


// Global list of arrays visited during toString, toLocaleString and
// join invocations.
var visited_arrays = new InternalArray();


// Gets a sorted array of array keys.  Useful for operations on sparse
// arrays.  Dupes have not been removed.
function GetSortedArrayKeys(array, indices) {
  var keys = new InternalArray();
  if (IS_NUMBER(indices)) {
    // It's an interval
    var limit = indices;
    for (var i = 0; i < limit; ++i) {
      var e = array[i];
      if (!IS_UNDEFINED(e) || i in array) {
        keys.push(i);
      }
    }
  } else {
    var length = indices.length;
    for (var k = 0; k < length; ++k) {
      var key = indices[k];
      if (!IS_UNDEFINED(key)) {
        var e = array[key];
        if (!IS_UNDEFINED(e) || key in array) {
          keys.push(key);
        }
      }
    }
    keys.sort(function(a, b) { return a - b; });
  }
  return keys;
}


function SparseJoinWithSeparatorJS(array, len, convert, separator) {
  var keys = GetSortedArrayKeys(array, %GetArrayKeys(array, len));
  var totalLength = 0;
  var elements = new InternalArray(keys.length * 2);
  var previousKey = -1;
  for (var i = 0; i < keys.length; i++) {
    var key = keys[i];
    if (key != previousKey) {  // keys may contain duplicates.
      var e = array[key];
      if (!IS_STRING(e)) e = convert(e);
      elements[i * 2] = key;
      elements[i * 2 + 1] = e;
      previousKey = key;
    }
  }
  return %SparseJoinWithSeparator(elements, len, separator);
}


// Optimized for sparse arrays if separator is ''.
function SparseJoin(array, len, convert) {
  var keys = GetSortedArrayKeys(array, %GetArrayKeys(array, len));
  var last_key = -1;
  var keys_length = keys.length;

  var elements = new InternalArray(keys_length);
  var elements_length = 0;

  for (var i = 0; i < keys_length; i++) {
    var key = keys[i];
    if (key != last_key) {
      var e = array[key];
      if (!IS_STRING(e)) e = convert(e);
      elements[elements_length++] = e;
      last_key = key;
    }
  }
  return %StringBuilderConcat(elements, elements_length, '');
}


function UseSparseVariant(array, length, is_array, touched) {
  // Only use the sparse variant on arrays that are likely to be sparse and the
  // number of elements touched in the operation is relatively small compared to
  // the overall size of the array.
  if (!is_array || length < 1000 || %IsObserved(array) ||
      %HasComplexElements(array)) {
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


function Join(array, length, separator, convert) {
  if (length == 0) return '';

  var is_array = IS_ARRAY(array);

  if (is_array) {
    // If the array is cyclic, return the empty string for already
    // visited arrays.
    if (!%PushIfAbsent(visited_arrays, array)) return '';
  }

  // Attempt to convert the elements.
  try {
    if (UseSparseVariant(array, length, is_array, length)) {
      %NormalizeElements(array);
      if (separator.length == 0) {
        return SparseJoin(array, length, convert);
      } else {
        return SparseJoinWithSeparatorJS(array, length, convert, separator);
      }
    }

    // Fast case for one-element arrays.
    if (length == 1) {
      var e = array[0];
      if (IS_STRING(e)) return e;
      return convert(e);
    }

    // Construct an array for the elements.
    var elements = new InternalArray(length);

    // We pull the empty separator check outside the loop for speed!
    if (separator.length == 0) {
      var elements_length = 0;
      for (var i = 0; i < length; i++) {
        var e = array[i];
        if (!IS_STRING(e)) e = convert(e);
        elements[elements_length++] = e;
      }
      elements.length = elements_length;
      var result = %_FastOneByteArrayJoin(elements, '');
      if (!IS_UNDEFINED(result)) return result;
      return %StringBuilderConcat(elements, elements_length, '');
    }
    // Non-empty separator case.
    // If the first element is a number then use the heuristic that the
    // remaining elements are also likely to be numbers.
    if (!IS_NUMBER(array[0])) {
      for (var i = 0; i < length; i++) {
        var e = array[i];
        if (!IS_STRING(e)) e = convert(e);
        elements[i] = e;
      }
    } else {
      for (var i = 0; i < length; i++) {
        var e = array[i];
        if (IS_NUMBER(e)) {
          e = %_NumberToString(e);
        } else if (!IS_STRING(e)) {
          e = convert(e);
        }
        elements[i] = e;
      }
    }
    var result = %_FastOneByteArrayJoin(elements, separator);
    if (!IS_UNDEFINED(result)) return result;

    return %StringBuilderJoin(elements, length, separator);
  } finally {
    // Make sure to remove the last element of the visited array no
    // matter what happens.
    if (is_array) visited_arrays.length = visited_arrays.length - 1;
  }
}


function ConvertToString(x) {
  if (IS_NULL_OR_UNDEFINED(x)) {
    return '';
  } else {
    return TO_STRING(x);
  }
}


function ConvertToLocaleString(e) {
  if (IS_NULL_OR_UNDEFINED(e)) {
    return '';
  } else {
    return TO_STRING(e.toLocaleString());
  }
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
        DefineIndexedProperty(deleted_elements, i - start_i, current);
      }
    }
  } else {
    var length = indices.length;
    for (var k = 0; k < length; ++k) {
      var key = indices[k];
      if (!IS_UNDEFINED(key)) {
        if (key >= start_i) {
          var current = array[key];
          if (!IS_UNDEFINED(current) || key in array) {
            DefineIndexedProperty(deleted_elements, key - start_i, current);
          }
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
      MinSimple(len - del_count + num_additional_args, 0xffffffff));
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
      if (!IS_UNDEFINED(key)) {
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
  var is_array = IS_ARRAY(array);
  for (var i = 0; i < del_count; i++) {
    var index = start_i + i;
    if (HAS_INDEX(array, index, is_array)) {
      var current = array[index];
      DefineIndexedProperty(deleted_elements, i, current);
    }
  }
}


function SimpleMove(array, start_i, del_count, len, num_additional_args) {
  var is_array = IS_ARRAY(array);
  if (num_additional_args !== del_count) {
    // Move the existing elements after the elements to be deleted
    // to the right position in the resulting array.
    if (num_additional_args > del_count) {
      for (var i = len - del_count; i > start_i; i--) {
        var from_index = i + del_count - 1;
        var to_index = i + num_additional_args - 1;
        if (HAS_INDEX(array, from_index, is_array)) {
          array[to_index] = array[from_index];
        } else {
          delete array[to_index];
        }
      }
    } else {
      for (var i = start_i; i < len - del_count; i++) {
        var from_index = i + del_count;
        var to_index = i + num_additional_args;
        if (HAS_INDEX(array, from_index, is_array)) {
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


function ArrayToString() {
  var array;
  var func;
  if (IS_ARRAY(this)) {
    func = this.join;
    if (func === ArrayJoin) {
      return Join(this, this.length, ',', ConvertToString);
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


function InnerArrayToLocaleString(array, length) {
  var len = TO_LENGTH_OR_UINT32(length);
  if (len === 0) return "";
  return Join(array, len, ',', ConvertToLocaleString);
}


function ArrayToLocaleString() {
  var array = TO_OBJECT(this);
  var arrayLen = array.length;
  return InnerArrayToLocaleString(array, arrayLen);
}


function InnerArrayJoin(separator, array, length) {
  if (IS_UNDEFINED(separator)) {
    separator = ',';
  } else {
    separator = TO_STRING(separator);
  }

  var result = %_FastOneByteArrayJoin(array, separator);
  if (!IS_UNDEFINED(result)) return result;

  // Fast case for one-element arrays.
  if (length === 1) {
    var e = array[0];
    if (IS_NULL_OR_UNDEFINED(e)) return '';
    return TO_STRING(e);
  }

  return Join(array, length, separator, ConvertToString);
}


function ArrayJoin(separator) {
  CHECK_OBJECT_COERCIBLE(this, "Array.prototype.join");

  var array = TO_OBJECT(this);
  var length = TO_LENGTH_OR_UINT32(array.length);

  return InnerArrayJoin(separator, array, length);
}


function ObservedArrayPop(n) {
  n--;
  var value = this[n];

  try {
    ObserveBeginPerformSplice(this);
    delete this[n];
    this.length = n;
  } finally {
    ObserveEndPerformSplice(this);
    ObserveEnqueueSpliceRecord(this, n, [value], 0);
  }

  return value;
}


// Removes the last element from the array and returns it. See
// ECMA-262, section 15.4.4.6.
function ArrayPop() {
  CHECK_OBJECT_COERCIBLE(this, "Array.prototype.pop");

  var array = TO_OBJECT(this);
  var n = TO_LENGTH_OR_UINT32(array.length);
  if (n == 0) {
    array.length = n;
    return;
  }

  if (%IsObserved(array))
    return ObservedArrayPop.call(array, n);

  n--;
  var value = array[n];
  %DeleteProperty_Strict(array, n);
  array.length = n;
  return value;
}


function ObservedArrayPush() {
  var n = TO_LENGTH_OR_UINT32(this.length);
  var m = %_ArgumentsLength();

  try {
    ObserveBeginPerformSplice(this);
    for (var i = 0; i < m; i++) {
      this[i+n] = %_Arguments(i);
    }
    var new_length = n + m;
    this.length = new_length;
  } finally {
    ObserveEndPerformSplice(this);
    ObserveEnqueueSpliceRecord(this, n, [], m);
  }

  return new_length;
}


// Appends the arguments to the end of the array and returns the new
// length of the array. See ECMA-262, section 15.4.4.7.
function ArrayPush() {
  CHECK_OBJECT_COERCIBLE(this, "Array.prototype.push");

  if (%IsObserved(this))
    return ObservedArrayPush.apply(this, arguments);

  var array = TO_OBJECT(this);
  var n = TO_LENGTH_OR_UINT32(array.length);
  var m = %_ArgumentsLength();

  // It appears that there is no enforced, absolute limit on the number of
  // arguments, but it would surely blow the stack to use 2**30 or more.
  // To avoid integer overflow, do the comparison to the max safe integer
  // after subtracting 2**30 from both sides. (2**31 would seem like a
  // natural value, but it is negative in JS, and 2**32 is 1.)
  if (m > (1 << 30) || (n - (1 << 30)) + m > kMaxSafeInteger - (1 << 30)) {
    throw MakeTypeError(kPushPastSafeLength, m, n);
  }

  for (var i = 0; i < m; i++) {
    array[i+n] = %_Arguments(i);
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


function ArrayReverse() {
  CHECK_OBJECT_COERCIBLE(this, "Array.prototype.reverse");

  var array = TO_OBJECT(this);
  var len = TO_LENGTH_OR_UINT32(array.length);
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


function ObservedArrayShift(len) {
  var first = this[0];

  try {
    ObserveBeginPerformSplice(this);
    SimpleMove(this, 0, 1, len, 0);
    this.length = len - 1;
  } finally {
    ObserveEndPerformSplice(this);
    ObserveEnqueueSpliceRecord(this, 0, [first], 0);
  }

  return first;
}


function ArrayShift() {
  CHECK_OBJECT_COERCIBLE(this, "Array.prototype.shift");

  var array = TO_OBJECT(this);
  var len = TO_LENGTH_OR_UINT32(array.length);

  if (len === 0) {
    array.length = 0;
    return;
  }

  if (%object_is_sealed(array)) throw MakeTypeError(kArrayFunctionsOnSealed);

  if (%IsObserved(array))
    return ObservedArrayShift.call(array, len);

  var first = array[0];

  if (UseSparseVariant(array, len, IS_ARRAY(array), len)) {
    SparseMove(array, 0, 1, len, 0);
  } else {
    SimpleMove(array, 0, 1, len, 0);
  }

  array.length = len - 1;

  return first;
}


function ObservedArrayUnshift() {
  var len = TO_LENGTH_OR_UINT32(this.length);
  var num_arguments = %_ArgumentsLength();

  try {
    ObserveBeginPerformSplice(this);
    SimpleMove(this, 0, 0, len, num_arguments);
    for (var i = 0; i < num_arguments; i++) {
      this[i] = %_Arguments(i);
    }
    var new_length = len + num_arguments;
    this.length = new_length;
  } finally {
    ObserveEndPerformSplice(this);
    ObserveEnqueueSpliceRecord(this, 0, [], num_arguments);
  }

  return new_length;
}


function ArrayUnshift(arg1) {  // length == 1
  CHECK_OBJECT_COERCIBLE(this, "Array.prototype.unshift");

  if (%IsObserved(this))
    return ObservedArrayUnshift.apply(this, arguments);

  var array = TO_OBJECT(this);
  var len = TO_LENGTH_OR_UINT32(array.length);
  var num_arguments = %_ArgumentsLength();

  if (len > 0 && UseSparseVariant(array, len, IS_ARRAY(array), len) &&
      !%object_is_sealed(array)) {
    SparseMove(array, 0, 0, len, num_arguments);
  } else {
    SimpleMove(array, 0, 0, len, num_arguments);
  }

  for (var i = 0; i < num_arguments; i++) {
    array[i] = %_Arguments(i);
  }

  var new_length = len + num_arguments;
  array.length = new_length;
  return new_length;
}


function ArraySlice(start, end) {
  CHECK_OBJECT_COERCIBLE(this, "Array.prototype.slice");

  var array = TO_OBJECT(this);
  var len = TO_LENGTH_OR_UINT32(array.length);
  var start_i = TO_INTEGER(start);
  var end_i = len;

  if (!IS_UNDEFINED(end)) end_i = TO_INTEGER(end);

  if (start_i < 0) {
    start_i += len;
    if (start_i < 0) start_i = 0;
  } else {
    if (start_i > len) start_i = len;
  }

  if (end_i < 0) {
    end_i += len;
    if (end_i < 0) end_i = 0;
  } else {
    if (end_i > len) end_i = len;
  }

  var result = ArraySpeciesCreate(array, MaxSimple(end_i - start_i, 0));

  if (end_i < start_i) return result;

  if (UseSparseVariant(array, len, IS_ARRAY(array), end_i - start_i)) {
    %NormalizeElements(array);
    %NormalizeElements(result);
    SparseSlice(array, start_i, end_i - start_i, len, result);
  } else {
    SimpleSlice(array, start_i, end_i - start_i, len, result);
  }

  result.length = end_i - start_i;

  return result;
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


function ObservedArraySplice(start, delete_count) {
  var num_arguments = %_ArgumentsLength();
  var len = TO_LENGTH_OR_UINT32(this.length);
  var start_i = ComputeSpliceStartIndex(TO_INTEGER(start), len);
  var del_count = ComputeSpliceDeleteCount(delete_count, num_arguments, len,
                                           start_i);
  var deleted_elements = [];
  deleted_elements.length = del_count;
  var num_elements_to_add = num_arguments > 2 ? num_arguments - 2 : 0;

  try {
    ObserveBeginPerformSplice(this);

    SimpleSlice(this, start_i, del_count, len, deleted_elements);
    SimpleMove(this, start_i, del_count, len, num_elements_to_add);

    // Insert the arguments into the resulting array in
    // place of the deleted elements.
    var i = start_i;
    var arguments_index = 2;
    var arguments_length = %_ArgumentsLength();
    while (arguments_index < arguments_length) {
      this[i++] = %_Arguments(arguments_index++);
    }
    this.length = len - del_count + num_elements_to_add;

  } finally {
    ObserveEndPerformSplice(this);
    if (deleted_elements.length || num_elements_to_add) {
      ObserveEnqueueSpliceRecord(this,
                                 start_i,
                                 deleted_elements.slice(),
                                 num_elements_to_add);
    }
  }

  // Return the deleted elements.
  return deleted_elements;
}


function ArraySplice(start, delete_count) {
  CHECK_OBJECT_COERCIBLE(this, "Array.prototype.splice");

  if (%IsObserved(this))
    return ObservedArraySplice.apply(this, arguments);

  var num_arguments = %_ArgumentsLength();
  var array = TO_OBJECT(this);
  var len = TO_LENGTH_OR_UINT32(array.length);
  var start_i = ComputeSpliceStartIndex(TO_INTEGER(start), len);
  var del_count = ComputeSpliceDeleteCount(delete_count, num_arguments, len,
                                           start_i);
  var deleted_elements = ArraySpeciesCreate(array, del_count);
  deleted_elements.length = del_count;
  var num_elements_to_add = num_arguments > 2 ? num_arguments - 2 : 0;

  if (del_count != num_elements_to_add && %object_is_sealed(array)) {
    throw MakeTypeError(kArrayFunctionsOnSealed);
  } else if (del_count > 0 && %object_is_frozen(array)) {
    throw MakeTypeError(kArrayFunctionsOnFrozen);
  }

  var changed_elements = del_count;
  if (num_elements_to_add != del_count) {
    // If the slice needs to do a actually move elements after the insertion
    // point, then include those in the estimate of changed elements.
    changed_elements += len - start_i - del_count;
  }
  if (UseSparseVariant(array, len, IS_ARRAY(array), changed_elements)) {
    %NormalizeElements(array);
    %NormalizeElements(deleted_elements);
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
  var arguments_length = %_ArgumentsLength();
  while (arguments_index < arguments_length) {
    array[i++] = %_Arguments(arguments_index++);
  }
  array.length = len - del_count + num_elements_to_add;

  // Return the deleted elements.
  return deleted_elements;
}


function InnerArraySort(array, length, comparefn) {
  // In-place QuickSort algorithm.
  // For short (length <= 22) arrays, insertion sort is used for efficiency.

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
  var InsertionSort = function InsertionSort(a, from, to) {
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

  var GetThirdIndex = function(a, from, to) {
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

  var QuickSort = function QuickSort(a, from, to) {
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
  var CopyFromPrototype = function CopyFromPrototype(obj, length) {
    var max = 0;
    for (var proto = %_GetPrototype(obj); proto; proto = %_GetPrototype(proto)) {
      var indices = %GetArrayKeys(proto, length);
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
          if (!IS_UNDEFINED(index) && !HAS_OWN_PROPERTY(obj, index)
              && HAS_OWN_PROPERTY(proto, index)) {
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
  var ShadowPrototypeElements = function(obj, from, to) {
    for (var proto = %_GetPrototype(obj); proto; proto = %_GetPrototype(proto)) {
      var indices = %GetArrayKeys(proto, to);
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
          if (!IS_UNDEFINED(index) && from <= index &&
              HAS_OWN_PROPERTY(proto, index)) {
            obj[index] = UNDEFINED;
          }
        }
      }
    }
  };

  var SafeRemoveArrayHoles = function SafeRemoveArrayHoles(obj) {
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
      // For compatability with Webkit, do not expose elements in the prototype.
      if (i in %_GetPrototype(obj)) {
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
    // The array is observed, or there were indexed accessors in the array.
    // Move array holes and undefineds to the end using a Javascript function
    // that is safe in the presence of accessors and is observable.
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


function ArraySort(comparefn) {
  CHECK_OBJECT_COERCIBLE(this, "Array.prototype.sort");

  var array = TO_OBJECT(this);
  var length = TO_LENGTH_OR_UINT32(array.length);
  return InnerArraySort(array, length, comparefn);
}


// The following functions cannot be made efficient on sparse arrays while
// preserving the semantics, since the calls to the receiver function can add
// or delete elements from the array.
function InnerArrayFilter(f, receiver, array, length, result) {
  var result_length = 0;
  var is_array = IS_ARRAY(array);
  for (var i = 0; i < length; i++) {
    if (HAS_INDEX(array, i, is_array)) {
      var element = array[i];
      if (%_Call(f, receiver, element, i, array)) {
        DefineIndexedProperty(result, result_length, element);
        result_length++;
      }
    }
  }
  return result;
}



function ArrayFilter(f, receiver) {
  CHECK_OBJECT_COERCIBLE(this, "Array.prototype.filter");

  // Pull out the length so that modifications to the length in the
  // loop will not affect the looping and side effects are visible.
  var array = TO_OBJECT(this);
  var length = TO_LENGTH_OR_UINT32(array.length);
  if (!IS_CALLABLE(f)) throw MakeTypeError(kCalledNonCallable, f);
  var result = ArraySpeciesCreate(array, 0);
  return InnerArrayFilter(f, receiver, array, length, result);
}


function InnerArrayForEach(f, receiver, array, length) {
  if (!IS_CALLABLE(f)) throw MakeTypeError(kCalledNonCallable, f);

  var is_array = IS_ARRAY(array);
  for (var i = 0; i < length; i++) {
    if (HAS_INDEX(array, i, is_array)) {
      var element = array[i];
      %_Call(f, receiver, element, i, array);
    }
  }
}


function ArrayForEach(f, receiver) {
  CHECK_OBJECT_COERCIBLE(this, "Array.prototype.forEach");

  // Pull out the length so that modifications to the length in the
  // loop will not affect the looping and side effects are visible.
  var array = TO_OBJECT(this);
  var length = TO_LENGTH_OR_UINT32(array.length);
  InnerArrayForEach(f, receiver, array, length);
}


function InnerArraySome(f, receiver, array, length) {
  if (!IS_CALLABLE(f)) throw MakeTypeError(kCalledNonCallable, f);

  var is_array = IS_ARRAY(array);
  for (var i = 0; i < length; i++) {
    if (HAS_INDEX(array, i, is_array)) {
      var element = array[i];
      if (%_Call(f, receiver, element, i, array)) return true;
    }
  }
  return false;
}


// Executes the function once for each element present in the
// array until it finds one where callback returns true.
function ArraySome(f, receiver) {
  CHECK_OBJECT_COERCIBLE(this, "Array.prototype.some");

  // Pull out the length so that modifications to the length in the
  // loop will not affect the looping and side effects are visible.
  var array = TO_OBJECT(this);
  var length = TO_LENGTH_OR_UINT32(array.length);
  return InnerArraySome(f, receiver, array, length);
}


function InnerArrayEvery(f, receiver, array, length) {
  if (!IS_CALLABLE(f)) throw MakeTypeError(kCalledNonCallable, f);

  var is_array = IS_ARRAY(array);
  for (var i = 0; i < length; i++) {
    if (HAS_INDEX(array, i, is_array)) {
      var element = array[i];
      if (!%_Call(f, receiver, element, i, array)) return false;
    }
  }
  return true;
}

function ArrayEvery(f, receiver) {
  CHECK_OBJECT_COERCIBLE(this, "Array.prototype.every");

  // Pull out the length so that modifications to the length in the
  // loop will not affect the looping and side effects are visible.
  var array = TO_OBJECT(this);
  var length = TO_LENGTH_OR_UINT32(array.length);
  return InnerArrayEvery(f, receiver, array, length);
}


function ArrayMap(f, receiver) {
  CHECK_OBJECT_COERCIBLE(this, "Array.prototype.map");

  // Pull out the length so that modifications to the length in the
  // loop will not affect the looping and side effects are visible.
  var array = TO_OBJECT(this);
  var length = TO_LENGTH_OR_UINT32(array.length);
  if (!IS_CALLABLE(f)) throw MakeTypeError(kCalledNonCallable, f);
  var result = ArraySpeciesCreate(array, length);
  var is_array = IS_ARRAY(array);
  for (var i = 0; i < length; i++) {
    if (HAS_INDEX(array, i, is_array)) {
      var element = array[i];
      DefineIndexedProperty(result, i, %_Call(f, receiver, element, i, array));
    }
  }
  return result;
}


// For .indexOf, we don't need to pass in the number of arguments
// at the callsite since ToInteger(undefined) == 0; however, for
// .lastIndexOf, we need to pass it, since the behavior for passing
// undefined is 0 but for not including the argument is length-1.
function InnerArrayIndexOf(array, element, index, length) {
  if (length == 0) return -1;
  if (IS_UNDEFINED(index)) {
    index = 0;
  } else {
    index = TO_INTEGER(index);
    // If index is negative, index from the end of the array.
    if (index < 0) {
      index = length + index;
      // If index is still negative, search the entire array.
      if (index < 0) index = 0;
    }
  }
  var min = index;
  var max = length;
  if (UseSparseVariant(array, length, IS_ARRAY(array), max - min)) {
    %NormalizeElements(array);
    var indices = %GetArrayKeys(array, length);
    if (IS_NUMBER(indices)) {
      // It's an interval.
      max = indices;  // Capped by length already.
      // Fall through to loop below.
    } else {
      if (indices.length == 0) return -1;
      // Get all the keys in sorted order.
      var sortedKeys = GetSortedArrayKeys(array, indices);
      var n = sortedKeys.length;
      var i = 0;
      while (i < n && sortedKeys[i] < index) i++;
      while (i < n) {
        var key = sortedKeys[i];
        if (!IS_UNDEFINED(key) && array[key] === element) return key;
        i++;
      }
      return -1;
    }
  }
  // Lookup through the array.
  if (!IS_UNDEFINED(element)) {
    for (var i = min; i < max; i++) {
      if (array[i] === element) return i;
    }
    return -1;
  }
  // Lookup through the array.
  for (var i = min; i < max; i++) {
    if (IS_UNDEFINED(array[i]) && i in array) {
      return i;
    }
  }
  return -1;
}


function ArrayIndexOf(element, index) {
  CHECK_OBJECT_COERCIBLE(this, "Array.prototype.indexOf");

  var length = TO_LENGTH_OR_UINT32(this.length);
  return InnerArrayIndexOf(this, element, index, length);
}


function InnerArrayLastIndexOf(array, element, index, length, argumentsLength) {
  if (length == 0) return -1;
  if (argumentsLength < 2) {
    index = length - 1;
  } else {
    index = TO_INTEGER(index);
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
        if (!IS_UNDEFINED(key) && array[key] === element) return key;
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
}


function ArrayLastIndexOf(element, index) {
  CHECK_OBJECT_COERCIBLE(this, "Array.prototype.lastIndexOf");

  var length = TO_LENGTH_OR_UINT32(this.length);
  return InnerArrayLastIndexOf(this, element, index, length,
                        %_ArgumentsLength());
}


function InnerArrayReduce(callback, current, array, length, argumentsLength) {
  if (!IS_CALLABLE(callback)) {
    throw MakeTypeError(kCalledNonCallable, callback);
  }

  var is_array = IS_ARRAY(array);
  var i = 0;
  find_initial: if (argumentsLength < 2) {
    for (; i < length; i++) {
      if (HAS_INDEX(array, i, is_array)) {
        current = array[i++];
        break find_initial;
      }
    }
    throw MakeTypeError(kReduceNoInitial);
  }

  for (; i < length; i++) {
    if (HAS_INDEX(array, i, is_array)) {
      var element = array[i];
      current = callback(current, element, i, array);
    }
  }
  return current;
}


function ArrayReduce(callback, current) {
  CHECK_OBJECT_COERCIBLE(this, "Array.prototype.reduce");

  // Pull out the length so that modifications to the length in the
  // loop will not affect the looping and side effects are visible.
  var array = TO_OBJECT(this);
  var length = TO_LENGTH_OR_UINT32(array.length);
  return InnerArrayReduce(callback, current, array, length,
                          %_ArgumentsLength());
}


function InnerArrayReduceRight(callback, current, array, length,
                               argumentsLength) {
  if (!IS_CALLABLE(callback)) {
    throw MakeTypeError(kCalledNonCallable, callback);
  }

  var is_array = IS_ARRAY(array);
  var i = length - 1;
  find_initial: if (argumentsLength < 2) {
    for (; i >= 0; i--) {
      if (HAS_INDEX(array, i, is_array)) {
        current = array[i--];
        break find_initial;
      }
    }
    throw MakeTypeError(kReduceNoInitial);
  }

  for (; i >= 0; i--) {
    if (HAS_INDEX(array, i, is_array)) {
      var element = array[i];
      current = callback(current, element, i, array);
    }
  }
  return current;
}


function ArrayReduceRight(callback, current) {
  CHECK_OBJECT_COERCIBLE(this, "Array.prototype.reduceRight");

  // Pull out the length so that side effects are visible before the
  // callback function is checked.
  var array = TO_OBJECT(this);
  var length = TO_LENGTH_OR_UINT32(array.length);
  return InnerArrayReduceRight(callback, current, array, length,
                               %_ArgumentsLength());
}


function InnerArrayCopyWithin(target, start, end, array, length) {
  target = TO_INTEGER(target);
  var to;
  if (target < 0) {
    to = MaxSimple(length + target, 0);
  } else {
    to = MinSimple(target, length);
  }

  start = TO_INTEGER(start);
  var from;
  if (start < 0) {
    from = MaxSimple(length + start, 0);
  } else {
    from = MinSimple(start, length);
  }

  end = IS_UNDEFINED(end) ? length : TO_INTEGER(end);
  var final;
  if (end < 0) {
    final = MaxSimple(length + end, 0);
  } else {
    final = MinSimple(end, length);
  }

  var count = MinSimple(final - from, length - to);
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
}


// ES6 draft 03-17-15, section 22.1.3.3
function ArrayCopyWithin(target, start, end) {
  CHECK_OBJECT_COERCIBLE(this, "Array.prototype.copyWithin");

  var array = TO_OBJECT(this);
  var length = TO_LENGTH(array.length);

  return InnerArrayCopyWithin(target, start, end, array, length);
}


function InnerArrayFind(predicate, thisArg, array, length) {
  if (!IS_CALLABLE(predicate)) {
    throw MakeTypeError(kCalledNonCallable, predicate);
  }

  for (var i = 0; i < length; i++) {
    var element = array[i];
    if (%_Call(predicate, thisArg, element, i, array)) {
      return element;
    }
  }

  return;
}


// ES6 draft 07-15-13, section 15.4.3.23
function ArrayFind(predicate, thisArg) {
  CHECK_OBJECT_COERCIBLE(this, "Array.prototype.find");

  var array = TO_OBJECT(this);
  var length = TO_INTEGER(array.length);

  return InnerArrayFind(predicate, thisArg, array, length);
}


function InnerArrayFindIndex(predicate, thisArg, array, length) {
  if (!IS_CALLABLE(predicate)) {
    throw MakeTypeError(kCalledNonCallable, predicate);
  }

  for (var i = 0; i < length; i++) {
    var element = array[i];
    if (%_Call(predicate, thisArg, element, i, array)) {
      return i;
    }
  }

  return -1;
}


// ES6 draft 07-15-13, section 15.4.3.24
function ArrayFindIndex(predicate, thisArg) {
  CHECK_OBJECT_COERCIBLE(this, "Array.prototype.findIndex");

  var array = TO_OBJECT(this);
  var length = TO_INTEGER(array.length);

  return InnerArrayFindIndex(predicate, thisArg, array, length);
}


// ES6, draft 04-05-14, section 22.1.3.6
function InnerArrayFill(value, start, end, array, length) {
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
    throw MakeTypeError(kArrayFunctionsOnFrozen);
  }

  for (; i < end; i++)
    array[i] = value;
  return array;
}


// ES6, draft 04-05-14, section 22.1.3.6
function ArrayFill(value, start, end) {
  CHECK_OBJECT_COERCIBLE(this, "Array.prototype.fill");

  var array = TO_OBJECT(this);
  var length = TO_LENGTH_OR_UINT32(array.length);

  return InnerArrayFill(value, start, end, array, length);
}


function InnerArrayIncludes(searchElement, fromIndex, array, length) {
  if (length === 0) {
    return false;
  }

  var n = TO_INTEGER(fromIndex);

  var k;
  if (n >= 0) {
    k = n;
  } else {
    k = length + n;
    if (k < 0) {
      k = 0;
    }
  }

  while (k < length) {
    var elementK = array[k];
    if (SameValueZero(searchElement, elementK)) {
      return true;
    }

    ++k;
  }

  return false;
}


// ES2016 draft, section 22.1.3.11
function ArrayIncludes(searchElement, fromIndex) {
  CHECK_OBJECT_COERCIBLE(this, "Array.prototype.includes");

  var array = TO_OBJECT(this);
  var length = TO_LENGTH(array.length);

  return InnerArrayIncludes(searchElement, fromIndex, array, length);
}


function AddArrayElement(constructor, array, i, value) {
  if (constructor === GlobalArray) {
    AddIndexedProperty(array, i, value);
  } else {
    ObjectDefineProperty(array, i, {
      value: value, writable: true, configurable: true, enumerable: true
    });
  }
}


// ES6, draft 10-14-14, section 22.1.2.1
function ArrayFrom(arrayLike, mapfn, receiver) {
  var items = TO_OBJECT(arrayLike);
  var mapping = !IS_UNDEFINED(mapfn);

  if (mapping) {
    if (!IS_CALLABLE(mapfn)) {
      throw MakeTypeError(kCalledNonCallable, mapfn);
    }
  }

  var iterable = GetMethod(items, iteratorSymbol);
  var k;
  var result;
  var mappedValue;
  var nextValue;

  if (!IS_UNDEFINED(iterable)) {
    result = %IsConstructor(this) ? new this() : [];

    var iterator = GetIterator(items, iterable);

    k = 0;
    while (true) {
      var next = iterator.next();

      if (!IS_RECEIVER(next)) {
        throw MakeTypeError(kIteratorResultNotAnObject, next);
      }

      if (next.done) {
        result.length = k;
        return result;
      }

      nextValue = next.value;
      if (mapping) {
        mappedValue = %_Call(mapfn, receiver, nextValue, k);
      } else {
        mappedValue = nextValue;
      }
      AddArrayElement(this, result, k, mappedValue);
      k++;
    }
  } else {
    var len = TO_LENGTH(items.length);
    result = %IsConstructor(this) ? new this(len) : new GlobalArray(len);

    for (k = 0; k < len; ++k) {
      nextValue = items[k];
      if (mapping) {
        mappedValue = %_Call(mapfn, receiver, nextValue, k);
      } else {
        mappedValue = nextValue;
      }
      AddArrayElement(this, result, k, mappedValue);
    }

    result.length = k;
    return result;
  }
}


// ES6, draft 05-22-14, section 22.1.2.3
function ArrayOf() {
  var length = %_ArgumentsLength();
  var constructor = this;
  // TODO: Implement IsConstructor (ES6 section 7.2.5)
  var array = %IsConstructor(constructor) ? new constructor(length) : [];
  for (var i = 0; i < length; i++) {
    AddArrayElement(constructor, array, i, %_Arguments(i));
  }
  array.length = length;
  return array;
}

// -------------------------------------------------------------------

// Set up non-enumerable constructor property on the Array.prototype
// object.
%AddNamedProperty(GlobalArray.prototype, "constructor", GlobalArray,
                  DONT_ENUM);

// Set up unscopable properties on the Array.prototype object.
var unscopables = {
  __proto__: null,
  copyWithin: true,
  entries: true,
  fill: true,
  find: true,
  findIndex: true,
  keys: true,
};

%AddNamedProperty(GlobalArray.prototype, unscopablesSymbol, unscopables,
                  DONT_ENUM | READ_ONLY);

%FunctionSetLength(ArrayFrom, 1);

// Set up non-enumerable functions on the Array object.
utils.InstallFunctions(GlobalArray, DONT_ENUM, [
  "from", ArrayFrom,
  "of", ArrayOf
]);

var specialFunctions = %SpecialArrayFunctions();

var getFunction = function(name, jsBuiltin, len) {
  var f = jsBuiltin;
  if (specialFunctions.hasOwnProperty(name)) {
    f = specialFunctions[name];
  }
  if (!IS_UNDEFINED(len)) {
    %FunctionSetLength(f, len);
  }
  return f;
};

// Set up non-enumerable functions of the Array.prototype object and
// set their names.
// Manipulate the length of some of the functions to meet
// expectations set by ECMA-262 or Mozilla.
utils.InstallFunctions(GlobalArray.prototype, DONT_ENUM, [
  "toString", getFunction("toString", ArrayToString),
  "toLocaleString", getFunction("toLocaleString", ArrayToLocaleString),
  "join", getFunction("join", ArrayJoin),
  "pop", getFunction("pop", ArrayPop),
  "push", getFunction("push", ArrayPush, 1),
  "reverse", getFunction("reverse", ArrayReverse),
  "shift", getFunction("shift", ArrayShift),
  "unshift", getFunction("unshift", ArrayUnshift, 1),
  "slice", getFunction("slice", ArraySlice, 2),
  "splice", getFunction("splice", ArraySplice, 2),
  "sort", getFunction("sort", ArraySort),
  "filter", getFunction("filter", ArrayFilter, 1),
  "forEach", getFunction("forEach", ArrayForEach, 1),
  "some", getFunction("some", ArraySome, 1),
  "every", getFunction("every", ArrayEvery, 1),
  "map", getFunction("map", ArrayMap, 1),
  "indexOf", getFunction("indexOf", ArrayIndexOf, 1),
  "lastIndexOf", getFunction("lastIndexOf", ArrayLastIndexOf, 1),
  "reduce", getFunction("reduce", ArrayReduce, 1),
  "reduceRight", getFunction("reduceRight", ArrayReduceRight, 1),
  "copyWithin", getFunction("copyWithin", ArrayCopyWithin, 2),
  "find", getFunction("find", ArrayFind, 1),
  "findIndex", getFunction("findIndex", ArrayFindIndex, 1),
  "fill", getFunction("fill", ArrayFill, 1),
  "includes", getFunction("includes", ArrayIncludes, 1),
]);

%FinishArrayPrototypeSetup(GlobalArray.prototype);

// The internal Array prototype doesn't need to be fancy, since it's never
// exposed to user code.
// Adding only the functions that are actually used.
utils.SetUpLockedPrototype(InternalArray, GlobalArray(), [
  "indexOf", getFunction("indexOf", ArrayIndexOf),
  "join", getFunction("join", ArrayJoin),
  "pop", getFunction("pop", ArrayPop),
  "push", getFunction("push", ArrayPush),
  "shift", getFunction("shift", ArrayShift),
  "sort", getFunction("sort", ArraySort),
  "splice", getFunction("splice", ArraySplice)
]);

utils.SetUpLockedPrototype(InternalPackedArray, GlobalArray(), [
  "join", getFunction("join", ArrayJoin),
  "pop", getFunction("pop", ArrayPop),
  "push", getFunction("push", ArrayPush),
  "shift", getFunction("shift", ArrayShift)
]);

// V8 extras get a separate copy of InternalPackedArray. We give them the basic
// manipulation methods.
utils.SetUpLockedPrototype(extrasUtils.InternalPackedArray, GlobalArray(), [
  "push", getFunction("push", ArrayPush),
  "pop", getFunction("pop", ArrayPop),
  "shift", getFunction("shift", ArrayShift),
  "unshift", getFunction("unshift", ArrayUnshift),
  "splice", getFunction("splice", ArraySplice),
  "slice", getFunction("slice", ArraySlice)
]);

// -------------------------------------------------------------------
// Exports

utils.Export(function(to) {
  to.ArrayFrom = ArrayFrom;
  to.ArrayIndexOf = ArrayIndexOf;
  to.ArrayJoin = ArrayJoin;
  to.ArrayPush = ArrayPush;
  to.ArrayToString = ArrayToString;
  to.InnerArrayCopyWithin = InnerArrayCopyWithin;
  to.InnerArrayEvery = InnerArrayEvery;
  to.InnerArrayFill = InnerArrayFill;
  to.InnerArrayFilter = InnerArrayFilter;
  to.InnerArrayFind = InnerArrayFind;
  to.InnerArrayFindIndex = InnerArrayFindIndex;
  to.InnerArrayForEach = InnerArrayForEach;
  to.InnerArrayIncludes = InnerArrayIncludes;
  to.InnerArrayIndexOf = InnerArrayIndexOf;
  to.InnerArrayJoin = InnerArrayJoin;
  to.InnerArrayLastIndexOf = InnerArrayLastIndexOf;
  to.InnerArrayReduce = InnerArrayReduce;
  to.InnerArrayReduceRight = InnerArrayReduceRight;
  to.InnerArraySome = InnerArraySome;
  to.InnerArraySort = InnerArraySort;
  to.InnerArrayToLocaleString = InnerArrayToLocaleString;
  to.PackedArrayReverse = PackedArrayReverse;
});

%InstallToContext([
  "array_pop", ArrayPop,
  "array_push", ArrayPush,
  "array_shift", ArrayShift,
  "array_splice", ArraySplice,
  "array_slice", ArraySlice,
  "array_unshift", ArrayUnshift,
]);

});
