// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"use strict";

// This file relies on the fact that the following declarations have been made
// in runtime.js:
// var $Array = global.Array;

// -------------------------------------------------------------------

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
    %_CallFunction(keys, function(a, b) { return a - b; }, ArraySort);
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
  if (!is_array || length < 1000 || %IsObserved(array)) {
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
      var result = %_FastAsciiArrayJoin(elements, '');
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
    var result = %_FastAsciiArrayJoin(elements, separator);
    if (!IS_UNDEFINED(result)) return result;

    return %StringBuilderJoin(elements, length, separator);
  } finally {
    // Make sure to remove the last element of the visited array no
    // matter what happens.
    if (is_array) visited_arrays.length = visited_arrays.length - 1;
  }
}


function ConvertToString(x) {
  // Assumes x is a non-string.
  if (IS_NUMBER(x)) return %_NumberToString(x);
  if (IS_BOOLEAN(x)) return x ? 'true' : 'false';
  return (IS_NULL_OR_UNDEFINED(x)) ? '' : %ToString(%DefaultString(x));
}


function ConvertToLocaleString(e) {
  if (IS_NULL_OR_UNDEFINED(e)) {
    return '';
  } else {
    // According to ES5, section 15.4.4.3, the toLocaleString conversion
    // must throw a TypeError if ToObject(e).toLocaleString isn't
    // callable.
    var e_obj = ToObject(e);
    return %ToString(e_obj.toLocaleString());
  }
}


// This function implements the optimized splice implementation that can use
// special array operations to handle sparse arrays in a sensible fashion.
function SmartSlice(array, start_i, del_count, len, deleted_elements) {
  // Move deleted elements to a new array (the return value from splice).
  var indices = %GetArrayKeys(array, start_i + del_count);
  if (IS_NUMBER(indices)) {
    var limit = indices;
    for (var i = start_i; i < limit; ++i) {
      var current = array[i];
      if (!IS_UNDEFINED(current) || i in array) {
        deleted_elements[i - start_i] = current;
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
            deleted_elements[key - start_i] = current;
          }
        }
      }
    }
  }
}


// This function implements the optimized splice implementation that can use
// special array operations to handle sparse arrays in a sensible fashion.
function SmartMove(array, start_i, del_count, len, num_additional_args) {
  // Move data to new array.
  var new_array = new InternalArray(len - del_count + num_additional_args);
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
            new_array[key - del_count + num_additional_args] = current;
          }
        }
      }
    }
  }
  // Move contents of new_array into this array
  %MoveArrayContents(new_array, array);
}


// This is part of the old simple-minded splice.  We are using it either
// because the receiver is not an array (so we have no choice) or because we
// know we are not deleting or moving a lot of elements.
function SimpleSlice(array, start_i, del_count, len, deleted_elements) {
  for (var i = 0; i < del_count; i++) {
    var index = start_i + i;
    // The spec could also be interpreted such that %HasOwnProperty
    // would be the appropriate test.  We follow KJS in consulting the
    // prototype.
    var current = array[index];
    if (!IS_UNDEFINED(current) || index in array) {
      deleted_elements[i] = current;
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
        // The spec could also be interpreted such that
        // %HasOwnProperty would be the appropriate test.  We follow
        // KJS in consulting the prototype.
        var current = array[from_index];
        if (!IS_UNDEFINED(current) || from_index in array) {
          array[to_index] = current;
        } else {
          delete array[to_index];
        }
      }
    } else {
      for (var i = start_i; i < len - del_count; i++) {
        var from_index = i + del_count;
        var to_index = i + num_additional_args;
        // The spec could also be interpreted such that
        // %HasOwnProperty would be the appropriate test.  We follow
        // KJS in consulting the prototype.
        var current = array[from_index];
        if (!IS_UNDEFINED(current) || from_index in array) {
          array[to_index] = current;
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
    array = ToObject(this);
    func = array.join;
  }
  if (!IS_SPEC_FUNCTION(func)) {
    return %_CallFunction(array, ObjectToString);
  }
  return %_CallFunction(array, func);
}


function ArrayToLocaleString() {
  var array = ToObject(this);
  var arrayLen = array.length;
  var len = TO_UINT32(arrayLen);
  if (len === 0) return "";
  return Join(array, len, ',', ConvertToLocaleString);
}


function ArrayJoin(separator) {
  CHECK_OBJECT_COERCIBLE(this, "Array.prototype.join");

  var array = TO_OBJECT_INLINE(this);
  var length = TO_UINT32(array.length);
  if (IS_UNDEFINED(separator)) {
    separator = ',';
  } else if (!IS_STRING(separator)) {
    separator = NonStringToString(separator);
  }

  var result = %_FastAsciiArrayJoin(array, separator);
  if (!IS_UNDEFINED(result)) return result;

  return Join(array, length, separator, ConvertToString);
}


function ObservedArrayPop(n) {
  n--;
  var value = this[n];

  try {
    BeginPerformSplice(this);
    delete this[n];
    this.length = n;
  } finally {
    EndPerformSplice(this);
    EnqueueSpliceRecord(this, n, [value], 0);
  }

  return value;
}

// Removes the last element from the array and returns it. See
// ECMA-262, section 15.4.4.6.
function ArrayPop() {
  CHECK_OBJECT_COERCIBLE(this, "Array.prototype.pop");

  var array = TO_OBJECT_INLINE(this);
  var n = TO_UINT32(array.length);
  if (n == 0) {
    array.length = n;
    return;
  }

  if (%IsObserved(array))
    return ObservedArrayPop.call(array, n);

  n--;
  var value = array[n];
  Delete(array, ToName(n), true);
  array.length = n;
  return value;
}


function ObservedArrayPush() {
  var n = TO_UINT32(this.length);
  var m = %_ArgumentsLength();

  try {
    BeginPerformSplice(this);
    for (var i = 0; i < m; i++) {
      this[i+n] = %_Arguments(i);
    }
    var new_length = n + m;
    this.length = new_length;
  } finally {
    EndPerformSplice(this);
    EnqueueSpliceRecord(this, n, [], m);
  }

  return new_length;
}

// Appends the arguments to the end of the array and returns the new
// length of the array. See ECMA-262, section 15.4.4.7.
function ArrayPush() {
  CHECK_OBJECT_COERCIBLE(this, "Array.prototype.push");

  if (%IsObserved(this))
    return ObservedArrayPush.apply(this, arguments);

  var array = TO_OBJECT_INLINE(this);
  var n = TO_UINT32(array.length);
  var m = %_ArgumentsLength();

  for (var i = 0; i < m; i++) {
    array[i+n] = %_Arguments(i);
  }

  var new_length = n + m;
  array.length = new_length;
  return new_length;
}


// Returns an array containing the array elements of the object followed
// by the array elements of each argument in order. See ECMA-262,
// section 15.4.4.7.
function ArrayConcatJS(arg1) {  // length == 1
  CHECK_OBJECT_COERCIBLE(this, "Array.prototype.concat");

  var array = ToObject(this);
  var arg_count = %_ArgumentsLength();
  var arrays = new InternalArray(1 + arg_count);
  arrays[0] = array;
  for (var i = 0; i < arg_count; i++) {
    arrays[i + 1] = %_Arguments(i);
  }

  return %ArrayConcat(arrays);
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


function ArrayReverse() {
  CHECK_OBJECT_COERCIBLE(this, "Array.prototype.reverse");

  var array = TO_OBJECT_INLINE(this);
  var len = TO_UINT32(array.length);

  if (UseSparseVariant(array, len, IS_ARRAY(array), len)) {
    %NormalizeElements(array);
    SparseReverse(array, len);
    return array;
  }

  var j = len - 1;
  for (var i = 0; i < j; i++, j--) {
    var current_i = array[i];
    if (!IS_UNDEFINED(current_i) || i in array) {
      var current_j = array[j];
      if (!IS_UNDEFINED(current_j) || j in array) {
        array[i] = current_j;
        array[j] = current_i;
      } else {
        array[j] = current_i;
        delete array[i];
      }
    } else {
      var current_j = array[j];
      if (!IS_UNDEFINED(current_j) || j in array) {
        array[i] = current_j;
        delete array[j];
      }
    }
  }
  return array;
}


function ObservedArrayShift(len) {
  var first = this[0];

  try {
    BeginPerformSplice(this);
    SimpleMove(this, 0, 1, len, 0);
    this.length = len - 1;
  } finally {
    EndPerformSplice(this);
    EnqueueSpliceRecord(this, 0, [first], 0);
  }

  return first;
}

function ArrayShift() {
  CHECK_OBJECT_COERCIBLE(this, "Array.prototype.shift");

  var array = TO_OBJECT_INLINE(this);
  var len = TO_UINT32(array.length);

  if (len === 0) {
    array.length = 0;
    return;
  }

  if (ObjectIsSealed(array)) {
    throw MakeTypeError("array_functions_change_sealed",
                        ["Array.prototype.shift"]);
  }

  if (%IsObserved(array))
    return ObservedArrayShift.call(array, len);

  var first = array[0];

  if (IS_ARRAY(array)) {
    SmartMove(array, 0, 1, len, 0);
  } else {
    SimpleMove(array, 0, 1, len, 0);
  }

  array.length = len - 1;

  return first;
}

function ObservedArrayUnshift() {
  var len = TO_UINT32(this.length);
  var num_arguments = %_ArgumentsLength();

  try {
    BeginPerformSplice(this);
    SimpleMove(this, 0, 0, len, num_arguments);
    for (var i = 0; i < num_arguments; i++) {
      this[i] = %_Arguments(i);
    }
    var new_length = len + num_arguments;
    this.length = new_length;
  } finally {
    EndPerformSplice(this);
    EnqueueSpliceRecord(this, 0, [], num_arguments);
  }

  return new_length;
}

function ArrayUnshift(arg1) {  // length == 1
  CHECK_OBJECT_COERCIBLE(this, "Array.prototype.unshift");

  if (%IsObserved(this))
    return ObservedArrayUnshift.apply(this, arguments);

  var array = TO_OBJECT_INLINE(this);
  var len = TO_UINT32(array.length);
  var num_arguments = %_ArgumentsLength();
  var is_sealed = ObjectIsSealed(array);

  if (IS_ARRAY(array) && !is_sealed && len > 0) {
    SmartMove(array, 0, 0, len, num_arguments);
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

  var array = TO_OBJECT_INLINE(this);
  var len = TO_UINT32(array.length);
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

  var result = [];

  if (end_i < start_i) return result;

  if (UseSparseVariant(array, len, IS_ARRAY(array), end_i - start_i)) {
    %NormalizeElements(array);
    %NormalizeElements(result);
    SmartSlice(array, start_i, end_i - start_i, len, result);
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
  var len = TO_UINT32(this.length);
  var start_i = ComputeSpliceStartIndex(TO_INTEGER(start), len);
  var del_count = ComputeSpliceDeleteCount(delete_count, num_arguments, len,
                                           start_i);
  var deleted_elements = [];
  deleted_elements.length = del_count;
  var num_elements_to_add = num_arguments > 2 ? num_arguments - 2 : 0;

  try {
    BeginPerformSplice(this);

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
    EndPerformSplice(this);
    if (deleted_elements.length || num_elements_to_add) {
       EnqueueSpliceRecord(this,
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
  var array = TO_OBJECT_INLINE(this);
  var len = TO_UINT32(array.length);
  var start_i = ComputeSpliceStartIndex(TO_INTEGER(start), len);
  var del_count = ComputeSpliceDeleteCount(delete_count, num_arguments, len,
                                           start_i);
  var deleted_elements = [];
  deleted_elements.length = del_count;
  var num_elements_to_add = num_arguments > 2 ? num_arguments - 2 : 0;

  if (del_count != num_elements_to_add && ObjectIsSealed(array)) {
    throw MakeTypeError("array_functions_change_sealed",
                        ["Array.prototype.splice"]);
  } else if (del_count > 0 && ObjectIsFrozen(array)) {
    throw MakeTypeError("array_functions_on_frozen",
                        ["Array.prototype.splice"]);
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
    SmartSlice(array, start_i, del_count, len, deleted_elements);
    SmartMove(array, start_i, del_count, len, num_elements_to_add);
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


function ArraySort(comparefn) {
  CHECK_OBJECT_COERCIBLE(this, "Array.prototype.sort");

  // In-place QuickSort algorithm.
  // For short (length <= 22) arrays, insertion sort is used for efficiency.

  if (!IS_SPEC_FUNCTION(comparefn)) {
    comparefn = function (x, y) {
      if (x === y) return 0;
      if (%_IsSmi(x) && %_IsSmi(y)) {
        return %SmiLexicographicCompare(x, y);
      }
      x = ToString(x);
      y = ToString(y);
      if (x == y) return 0;
      else return x < y ? -1 : 1;
    };
  }
  var receiver = %GetDefaultReceiver(comparefn);

  var InsertionSort = function InsertionSort(a, from, to) {
    for (var i = from + 1; i < to; i++) {
      var element = a[i];
      for (var j = i - 1; j >= from; j--) {
        var tmp = a[j];
        var order = %_CallFunction(receiver, tmp, element, comparefn);
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
    var t_array = [];
    // Use both 'from' and 'to' to determine the pivot candidates.
    var increment = 200 + ((to - from) & 15);
    for (var i = from + 1; i < to - 1; i += increment) {
      t_array.push([i, a[i]]);
    }
    t_array.sort(function(a, b) {
        return %_CallFunction(receiver, a[1], b[1], comparefn) } );
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
      var c01 = %_CallFunction(receiver, v0, v1, comparefn);
      if (c01 > 0) {
        // v1 < v0, so swap them.
        var tmp = v0;
        v0 = v1;
        v1 = tmp;
      } // v0 <= v1.
      var c02 = %_CallFunction(receiver, v0, v2, comparefn);
      if (c02 >= 0) {
        // v2 <= v0 <= v1.
        var tmp = v0;
        v0 = v2;
        v2 = v1;
        v1 = tmp;
      } else {
        // v0 <= v1 && v0 < v2
        var c12 = %_CallFunction(receiver, v1, v2, comparefn);
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
        var order = %_CallFunction(receiver, element, pivot, comparefn);
        if (order < 0) {
          a[i] = a[low_end];
          a[low_end] = element;
          low_end++;
        } else if (order > 0) {
          do {
            high_start--;
            if (high_start == i) break partition;
            var top_elem = a[high_start];
            order = %_CallFunction(receiver, top_elem, pivot, comparefn);
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
    for (var proto = %GetPrototype(obj); proto; proto = %GetPrototype(proto)) {
      var indices = %GetArrayKeys(proto, length);
      if (IS_NUMBER(indices)) {
        // It's an interval.
        var proto_length = indices;
        for (var i = 0; i < proto_length; i++) {
          if (!obj.hasOwnProperty(i) && proto.hasOwnProperty(i)) {
            obj[i] = proto[i];
            if (i >= max) { max = i + 1; }
          }
        }
      } else {
        for (var i = 0; i < indices.length; i++) {
          var index = indices[i];
          if (!IS_UNDEFINED(index) &&
              !obj.hasOwnProperty(index) && proto.hasOwnProperty(index)) {
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
    for (var proto = %GetPrototype(obj); proto; proto = %GetPrototype(proto)) {
      var indices = %GetArrayKeys(proto, to);
      if (IS_NUMBER(indices)) {
        // It's an interval.
        var proto_length = indices;
        for (var i = from; i < proto_length; i++) {
          if (proto.hasOwnProperty(i)) {
            obj[i] = UNDEFINED;
          }
        }
      } else {
        for (var i = 0; i < indices.length; i++) {
          var index = indices[i];
          if (!IS_UNDEFINED(index) && from <= index &&
              proto.hasOwnProperty(index)) {
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
      if (!obj.hasOwnProperty(first_undefined)) {
        num_holes++;
      }

      // Find last defined element.
      while (first_undefined < last_defined &&
             IS_UNDEFINED(obj[last_defined])) {
        if (!obj.hasOwnProperty(last_defined)) {
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
      if (i in %GetPrototype(obj)) {
        obj[i] = UNDEFINED;
      } else {
        delete obj[i];
      }
    }

    // Return the number of defined elements.
    return first_undefined;
  };

  var length = TO_UINT32(this.length);
  if (length < 2) return this;

  var is_array = IS_ARRAY(this);
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
    max_prototype_element = CopyFromPrototype(this, length);
  }

  // %RemoveArrayHoles returns -1 if fast removal is not supported.
  var num_non_undefined = %RemoveArrayHoles(this, length);

  if (num_non_undefined == -1) {
    // The array is observed, or there were indexed accessors in the array.
    // Move array holes and undefineds to the end using a Javascript function
    // that is safe in the presence of accessors and is observable.
    num_non_undefined = SafeRemoveArrayHoles(this);
  }

  QuickSort(this, 0, num_non_undefined);

  if (!is_array && (num_non_undefined + 1 < max_prototype_element)) {
    // For compatibility with JSC, we shadow any elements in the prototype
    // chain that has become exposed by sort moving a hole to its position.
    ShadowPrototypeElements(this, num_non_undefined, max_prototype_element);
  }

  return this;
}


// The following functions cannot be made efficient on sparse arrays while
// preserving the semantics, since the calls to the receiver function can add
// or delete elements from the array.
function ArrayFilter(f, receiver) {
  CHECK_OBJECT_COERCIBLE(this, "Array.prototype.filter");

  // Pull out the length so that modifications to the length in the
  // loop will not affect the looping and side effects are visible.
  var array = ToObject(this);
  var length = ToUint32(array.length);

  if (!IS_SPEC_FUNCTION(f)) {
    throw MakeTypeError('called_non_callable', [ f ]);
  }
  if (IS_NULL_OR_UNDEFINED(receiver)) {
    receiver = %GetDefaultReceiver(f) || receiver;
  } else if (!IS_SPEC_OBJECT(receiver) && %IsSloppyModeFunction(f)) {
    receiver = ToObject(receiver);
  }

  var result = new $Array();
  var accumulator = new InternalArray();
  var accumulator_length = 0;
  var stepping = DEBUG_IS_ACTIVE && %DebugCallbackSupportsStepping(f);
  for (var i = 0; i < length; i++) {
    if (i in array) {
      var element = array[i];
      // Prepare break slots for debugger step in.
      if (stepping) %DebugPrepareStepInIfStepping(f);
      if (%_CallFunction(receiver, element, i, array, f)) {
        accumulator[accumulator_length++] = element;
      }
    }
  }
  %MoveArrayContents(accumulator, result);
  return result;
}


function ArrayForEach(f, receiver) {
  CHECK_OBJECT_COERCIBLE(this, "Array.prototype.forEach");

  // Pull out the length so that modifications to the length in the
  // loop will not affect the looping and side effects are visible.
  var array = ToObject(this);
  var length = TO_UINT32(array.length);

  if (!IS_SPEC_FUNCTION(f)) {
    throw MakeTypeError('called_non_callable', [ f ]);
  }
  if (IS_NULL_OR_UNDEFINED(receiver)) {
    receiver = %GetDefaultReceiver(f) || receiver;
  } else if (!IS_SPEC_OBJECT(receiver) && %IsSloppyModeFunction(f)) {
    receiver = ToObject(receiver);
  }

  var stepping = DEBUG_IS_ACTIVE && %DebugCallbackSupportsStepping(f);
  for (var i = 0; i < length; i++) {
    if (i in array) {
      var element = array[i];
      // Prepare break slots for debugger step in.
      if (stepping) %DebugPrepareStepInIfStepping(f);
      %_CallFunction(receiver, element, i, array, f);
    }
  }
}


// Executes the function once for each element present in the
// array until it finds one where callback returns true.
function ArraySome(f, receiver) {
  CHECK_OBJECT_COERCIBLE(this, "Array.prototype.some");

  // Pull out the length so that modifications to the length in the
  // loop will not affect the looping and side effects are visible.
  var array = ToObject(this);
  var length = TO_UINT32(array.length);

  if (!IS_SPEC_FUNCTION(f)) {
    throw MakeTypeError('called_non_callable', [ f ]);
  }
  if (IS_NULL_OR_UNDEFINED(receiver)) {
    receiver = %GetDefaultReceiver(f) || receiver;
  } else if (!IS_SPEC_OBJECT(receiver) && %IsSloppyModeFunction(f)) {
    receiver = ToObject(receiver);
  }

  var stepping = DEBUG_IS_ACTIVE && %DebugCallbackSupportsStepping(f);
  for (var i = 0; i < length; i++) {
    if (i in array) {
      var element = array[i];
      // Prepare break slots for debugger step in.
      if (stepping) %DebugPrepareStepInIfStepping(f);
      if (%_CallFunction(receiver, element, i, array, f)) return true;
    }
  }
  return false;
}


function ArrayEvery(f, receiver) {
  CHECK_OBJECT_COERCIBLE(this, "Array.prototype.every");

  // Pull out the length so that modifications to the length in the
  // loop will not affect the looping and side effects are visible.
  var array = ToObject(this);
  var length = TO_UINT32(array.length);

  if (!IS_SPEC_FUNCTION(f)) {
    throw MakeTypeError('called_non_callable', [ f ]);
  }
  if (IS_NULL_OR_UNDEFINED(receiver)) {
    receiver = %GetDefaultReceiver(f) || receiver;
  } else if (!IS_SPEC_OBJECT(receiver) && %IsSloppyModeFunction(f)) {
    receiver = ToObject(receiver);
  }

  var stepping = DEBUG_IS_ACTIVE && %DebugCallbackSupportsStepping(f);
  for (var i = 0; i < length; i++) {
    if (i in array) {
      var element = array[i];
      // Prepare break slots for debugger step in.
      if (stepping) %DebugPrepareStepInIfStepping(f);
      if (!%_CallFunction(receiver, element, i, array, f)) return false;
    }
  }
  return true;
}

function ArrayMap(f, receiver) {
  CHECK_OBJECT_COERCIBLE(this, "Array.prototype.map");

  // Pull out the length so that modifications to the length in the
  // loop will not affect the looping and side effects are visible.
  var array = ToObject(this);
  var length = TO_UINT32(array.length);

  if (!IS_SPEC_FUNCTION(f)) {
    throw MakeTypeError('called_non_callable', [ f ]);
  }
  if (IS_NULL_OR_UNDEFINED(receiver)) {
    receiver = %GetDefaultReceiver(f) || receiver;
  } else if (!IS_SPEC_OBJECT(receiver) && %IsSloppyModeFunction(f)) {
    receiver = ToObject(receiver);
  }

  var result = new $Array();
  var accumulator = new InternalArray(length);
  var stepping = DEBUG_IS_ACTIVE && %DebugCallbackSupportsStepping(f);
  for (var i = 0; i < length; i++) {
    if (i in array) {
      var element = array[i];
      // Prepare break slots for debugger step in.
      if (stepping) %DebugPrepareStepInIfStepping(f);
      accumulator[i] = %_CallFunction(receiver, element, i, array, f);
    }
  }
  %MoveArrayContents(accumulator, result);
  return result;
}


function ArrayIndexOf(element, index) {
  CHECK_OBJECT_COERCIBLE(this, "Array.prototype.indexOf");

  var length = TO_UINT32(this.length);
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
  if (UseSparseVariant(this, length, IS_ARRAY(this), max - min)) {
    %NormalizeElements(this);
    var indices = %GetArrayKeys(this, length);
    if (IS_NUMBER(indices)) {
      // It's an interval.
      max = indices;  // Capped by length already.
      // Fall through to loop below.
    } else {
      if (indices.length == 0) return -1;
      // Get all the keys in sorted order.
      var sortedKeys = GetSortedArrayKeys(this, indices);
      var n = sortedKeys.length;
      var i = 0;
      while (i < n && sortedKeys[i] < index) i++;
      while (i < n) {
        var key = sortedKeys[i];
        if (!IS_UNDEFINED(key) && this[key] === element) return key;
        i++;
      }
      return -1;
    }
  }
  // Lookup through the array.
  if (!IS_UNDEFINED(element)) {
    for (var i = min; i < max; i++) {
      if (this[i] === element) return i;
    }
    return -1;
  }
  // Lookup through the array.
  for (var i = min; i < max; i++) {
    if (IS_UNDEFINED(this[i]) && i in this) {
      return i;
    }
  }
  return -1;
}


function ArrayLastIndexOf(element, index) {
  CHECK_OBJECT_COERCIBLE(this, "Array.prototype.lastIndexOf");

  var length = TO_UINT32(this.length);
  if (length == 0) return -1;
  if (%_ArgumentsLength() < 2) {
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
  if (UseSparseVariant(this, length, IS_ARRAY(this), index)) {
    %NormalizeElements(this);
    var indices = %GetArrayKeys(this, index + 1);
    if (IS_NUMBER(indices)) {
      // It's an interval.
      max = indices;  // Capped by index already.
      // Fall through to loop below.
    } else {
      if (indices.length == 0) return -1;
      // Get all the keys in sorted order.
      var sortedKeys = GetSortedArrayKeys(this, indices);
      var i = sortedKeys.length - 1;
      while (i >= 0) {
        var key = sortedKeys[i];
        if (!IS_UNDEFINED(key) && this[key] === element) return key;
        i--;
      }
      return -1;
    }
  }
  // Lookup through the array.
  if (!IS_UNDEFINED(element)) {
    for (var i = max; i >= min; i--) {
      if (this[i] === element) return i;
    }
    return -1;
  }
  for (var i = max; i >= min; i--) {
    if (IS_UNDEFINED(this[i]) && i in this) {
      return i;
    }
  }
  return -1;
}


function ArrayReduce(callback, current) {
  CHECK_OBJECT_COERCIBLE(this, "Array.prototype.reduce");

  // Pull out the length so that modifications to the length in the
  // loop will not affect the looping and side effects are visible.
  var array = ToObject(this);
  var length = ToUint32(array.length);

  if (!IS_SPEC_FUNCTION(callback)) {
    throw MakeTypeError('called_non_callable', [callback]);
  }

  var i = 0;
  find_initial: if (%_ArgumentsLength() < 2) {
    for (; i < length; i++) {
      current = array[i];
      if (!IS_UNDEFINED(current) || i in array) {
        i++;
        break find_initial;
      }
    }
    throw MakeTypeError('reduce_no_initial', []);
  }

  var receiver = %GetDefaultReceiver(callback);
  var stepping = DEBUG_IS_ACTIVE && %DebugCallbackSupportsStepping(callback);
  for (; i < length; i++) {
    if (i in array) {
      var element = array[i];
      // Prepare break slots for debugger step in.
      if (stepping) %DebugPrepareStepInIfStepping(callback);
      current = %_CallFunction(receiver, current, element, i, array, callback);
    }
  }
  return current;
}

function ArrayReduceRight(callback, current) {
  CHECK_OBJECT_COERCIBLE(this, "Array.prototype.reduceRight");

  // Pull out the length so that side effects are visible before the
  // callback function is checked.
  var array = ToObject(this);
  var length = ToUint32(array.length);

  if (!IS_SPEC_FUNCTION(callback)) {
    throw MakeTypeError('called_non_callable', [callback]);
  }

  var i = length - 1;
  find_initial: if (%_ArgumentsLength() < 2) {
    for (; i >= 0; i--) {
      current = array[i];
      if (!IS_UNDEFINED(current) || i in array) {
        i--;
        break find_initial;
      }
    }
    throw MakeTypeError('reduce_no_initial', []);
  }

  var receiver = %GetDefaultReceiver(callback);
  var stepping = DEBUG_IS_ACTIVE && %DebugCallbackSupportsStepping(callback);
  for (; i >= 0; i--) {
    if (i in array) {
      var element = array[i];
      // Prepare break slots for debugger step in.
      if (stepping) %DebugPrepareStepInIfStepping(callback);
      current = %_CallFunction(receiver, current, element, i, array, callback);
    }
  }
  return current;
}

// ES5, 15.4.3.2
function ArrayIsArray(obj) {
  return IS_ARRAY(obj);
}


// -------------------------------------------------------------------

function SetUpArray() {
  %CheckIsBootstrapping();

  // Set up non-enumerable constructor property on the Array.prototype
  // object.
  %AddNamedProperty($Array.prototype, "constructor", $Array, DONT_ENUM);

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
  %AddNamedProperty($Array.prototype, symbolUnscopables, unscopables,
      DONT_ENUM | READ_ONLY);

  // Set up non-enumerable functions on the Array object.
  InstallFunctions($Array, DONT_ENUM, $Array(
    "isArray", ArrayIsArray
  ));

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
  InstallFunctions($Array.prototype, DONT_ENUM, $Array(
    "toString", getFunction("toString", ArrayToString),
    "toLocaleString", getFunction("toLocaleString", ArrayToLocaleString),
    "join", getFunction("join", ArrayJoin),
    "pop", getFunction("pop", ArrayPop),
    "push", getFunction("push", ArrayPush, 1),
    "concat", getFunction("concat", ArrayConcatJS, 1),
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
    "reduceRight", getFunction("reduceRight", ArrayReduceRight, 1)
  ));

  %FinishArrayPrototypeSetup($Array.prototype);

  // The internal Array prototype doesn't need to be fancy, since it's never
  // exposed to user code.
  // Adding only the functions that are actually used.
  SetUpLockedPrototype(InternalArray, $Array(), $Array(
    "concat", getFunction("concat", ArrayConcatJS),
    "indexOf", getFunction("indexOf", ArrayIndexOf),
    "join", getFunction("join", ArrayJoin),
    "pop", getFunction("pop", ArrayPop),
    "push", getFunction("push", ArrayPush),
    "splice", getFunction("splice", ArraySplice)
  ));

  SetUpLockedPrototype(InternalPackedArray, $Array(), $Array(
    "join", getFunction("join", ArrayJoin),
    "pop", getFunction("pop", ArrayPop),
    "push", getFunction("push", ArrayPush)
  ));
}

SetUpArray();
