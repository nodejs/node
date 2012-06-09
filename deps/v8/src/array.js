// Copyright 2010 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// This file relies on the fact that the following declarations have been made
// in runtime.js:
// var $Array = global.Array;

// -------------------------------------------------------------------

// Global list of arrays visited during toString, toLocaleString and
// join invocations.
var visited_arrays = new InternalArray();


// Gets a sorted array of array keys.  Useful for operations on sparse
// arrays.  Dupes have not been removed.
function GetSortedArrayKeys(array, intervals) {
  var length = intervals.length;
  var keys = [];
  for (var k = 0; k < length; k++) {
    var key = intervals[k];
    if (key < 0) {
      var j = -1 - key;
      var limit = j + intervals[++k];
      for (; j < limit; j++) {
        var e = array[j];
        if (!IS_UNDEFINED(e) || j in array) {
          keys.push(j);
        }
      }
    } else {
      // The case where key is undefined also ends here.
      if (!IS_UNDEFINED(key)) {
        var e = array[key];
        if (!IS_UNDEFINED(e) || key in array) {
          keys.push(key);
        }
      }
    }
  }
  keys.sort(function(a, b) { return a - b; });
  return keys;
}


function SparseJoinWithSeparator(array, len, convert, separator) {
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


function UseSparseVariant(object, length, is_array) {
   return is_array &&
       length > 1000 &&
       (!%_IsSmi(length) ||
        %EstimateNumberOfElements(object) < (length >> 2));
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
    if (UseSparseVariant(array, length, is_array)) {
      if (separator.length == 0) {
        return SparseJoin(array, length, convert);
      } else {
        return SparseJoinWithSeparator(array, length, convert, separator);
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
  // Intervals array can contain keys and intervals.  See comment in Concat.
  var intervals = %GetArrayKeys(array, start_i + del_count);
  var length = intervals.length;
  for (var k = 0; k < length; k++) {
    var key = intervals[k];
    if (key < 0) {
      var j = -1 - key;
      var interval_limit = j + intervals[++k];
      if (j < start_i) {
        j = start_i;
      }
      for (; j < interval_limit; j++) {
        // ECMA-262 15.4.4.12 line 10.  The spec could also be
        // interpreted such that %HasLocalProperty would be the
        // appropriate test.  We follow KJS in consulting the
        // prototype.
        var current = array[j];
        if (!IS_UNDEFINED(current) || j in array) {
          deleted_elements[j - start_i] = current;
        }
      }
    } else {
      if (!IS_UNDEFINED(key)) {
        if (key >= start_i) {
          // ECMA-262 15.4.4.12 line 10.  The spec could also be
          // interpreted such that %HasLocalProperty would be the
          // appropriate test.  We follow KJS in consulting the
          // prototype.
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
  var intervals = %GetArrayKeys(array, len);
  var length = intervals.length;
  for (var k = 0; k < length; k++) {
    var key = intervals[k];
    if (key < 0) {
      var j = -1 - key;
      var interval_limit = j + intervals[++k];
      while (j < start_i && j < interval_limit) {
        // The spec could also be interpreted such that
        // %HasLocalProperty would be the appropriate test.  We follow
        // KJS in consulting the prototype.
        var current = array[j];
        if (!IS_UNDEFINED(current) || j in array) {
          new_array[j] = current;
        }
        j++;
      }
      j = start_i + del_count;
      while (j < interval_limit) {
        // ECMA-262 15.4.4.12 lines 24 and 41.  The spec could also be
        // interpreted such that %HasLocalProperty would be the
        // appropriate test.  We follow KJS in consulting the
        // prototype.
        var current = array[j];
        if (!IS_UNDEFINED(current) || j in array) {
          new_array[j - del_count + num_additional_args] = current;
        }
        j++;
      }
    } else {
      if (!IS_UNDEFINED(key)) {
        if (key < start_i) {
          // The spec could also be interpreted such that
          // %HasLocalProperty would be the appropriate test.  We follow
          // KJS in consulting the prototype.
          var current = array[key];
          if (!IS_UNDEFINED(current) || key in array) {
            new_array[key] = current;
          }
        } else if (key >= start_i + del_count) {
          // ECMA-262 15.4.4.12 lines 24 and 41.  The spec could also
          // be interpreted such that %HasLocalProperty would be the
          // appropriate test.  We follow KJS in consulting the
          // prototype.
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
    // The spec could also be interpreted such that %HasLocalProperty
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
        // %HasLocalProperty would be the appropriate test.  We follow
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
        // %HasLocalProperty would be the appropriate test.  We follow
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
  if (IS_NULL_OR_UNDEFINED(this) && !IS_UNDETECTABLE(this)) {
    throw MakeTypeError("called_on_null_or_undefined",
                        ["Array.prototype.join"]);
  }

  if (IS_UNDEFINED(separator)) {
    separator = ',';
  } else if (!IS_STRING(separator)) {
    separator = NonStringToString(separator);
  }

  var result = %_FastAsciiArrayJoin(this, separator);
  if (!IS_UNDEFINED(result)) return result;

  return Join(this, TO_UINT32(this.length), separator, ConvertToString);
}


// Removes the last element from the array and returns it. See
// ECMA-262, section 15.4.4.6.
function ArrayPop() {
  if (IS_NULL_OR_UNDEFINED(this) && !IS_UNDETECTABLE(this)) {
    throw MakeTypeError("called_on_null_or_undefined",
                        ["Array.prototype.pop"]);
  }

  var n = TO_UINT32(this.length);
  if (n == 0) {
    this.length = n;
    return;
  }
  n--;
  var value = this[n];
  this.length = n;
  delete this[n];
  return value;
}


// Appends the arguments to the end of the array and returns the new
// length of the array. See ECMA-262, section 15.4.4.7.
function ArrayPush() {
  if (IS_NULL_OR_UNDEFINED(this) && !IS_UNDETECTABLE(this)) {
    throw MakeTypeError("called_on_null_or_undefined",
                        ["Array.prototype.push"]);
  }

  var n = TO_UINT32(this.length);
  var m = %_ArgumentsLength();
  for (var i = 0; i < m; i++) {
    this[i+n] = %_Arguments(i);
  }
  this.length = n + m;
  return this.length;
}


function ArrayConcat(arg1) {  // length == 1
  if (IS_NULL_OR_UNDEFINED(this) && !IS_UNDETECTABLE(this)) {
    throw MakeTypeError("called_on_null_or_undefined",
                        ["Array.prototype.concat"]);
  }

  var arg_count = %_ArgumentsLength();
  var arrays = new InternalArray(1 + arg_count);
  arrays[0] = this;
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
  if (IS_NULL_OR_UNDEFINED(this) && !IS_UNDETECTABLE(this)) {
    throw MakeTypeError("called_on_null_or_undefined",
                        ["Array.prototype.reverse"]);
  }

  var j = TO_UINT32(this.length) - 1;

  if (UseSparseVariant(this, j, IS_ARRAY(this))) {
    SparseReverse(this, j+1);
    return this;
  }

  for (var i = 0; i < j; i++, j--) {
    var current_i = this[i];
    if (!IS_UNDEFINED(current_i) || i in this) {
      var current_j = this[j];
      if (!IS_UNDEFINED(current_j) || j in this) {
        this[i] = current_j;
        this[j] = current_i;
      } else {
        this[j] = current_i;
        delete this[i];
      }
    } else {
      var current_j = this[j];
      if (!IS_UNDEFINED(current_j) || j in this) {
        this[i] = current_j;
        delete this[j];
      }
    }
  }
  return this;
}


function ArrayShift() {
  if (IS_NULL_OR_UNDEFINED(this) && !IS_UNDETECTABLE(this)) {
    throw MakeTypeError("called_on_null_or_undefined",
                        ["Array.prototype.shift"]);
  }

  var len = TO_UINT32(this.length);

  if (len === 0) {
    this.length = 0;
    return;
  }

  var first = this[0];

  if (IS_ARRAY(this)) {
    SmartMove(this, 0, 1, len, 0);
  } else {
    SimpleMove(this, 0, 1, len, 0);
  }

  this.length = len - 1;

  return first;
}


function ArrayUnshift(arg1) {  // length == 1
  if (IS_NULL_OR_UNDEFINED(this) && !IS_UNDETECTABLE(this)) {
    throw MakeTypeError("called_on_null_or_undefined",
                        ["Array.prototype.unshift"]);
  }

  var len = TO_UINT32(this.length);
  var num_arguments = %_ArgumentsLength();

  if (IS_ARRAY(this)) {
    SmartMove(this, 0, 0, len, num_arguments);
  } else {
    SimpleMove(this, 0, 0, len, num_arguments);
  }

  for (var i = 0; i < num_arguments; i++) {
    this[i] = %_Arguments(i);
  }

  this.length = len + num_arguments;

  return len + num_arguments;
}


function ArraySlice(start, end) {
  if (IS_NULL_OR_UNDEFINED(this) && !IS_UNDETECTABLE(this)) {
    throw MakeTypeError("called_on_null_or_undefined",
                        ["Array.prototype.slice"]);
  }

  var len = TO_UINT32(this.length);
  var start_i = TO_INTEGER(start);
  var end_i = len;

  if (end !== void 0) end_i = TO_INTEGER(end);

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

  if (IS_ARRAY(this) &&
      (end_i > 1000) &&
      (%EstimateNumberOfElements(this) < end_i)) {
    SmartSlice(this, start_i, end_i - start_i, len, result);
  } else {
    SimpleSlice(this, start_i, end_i - start_i, len, result);
  }

  result.length = end_i - start_i;

  return result;
}


function ArraySplice(start, delete_count) {
  if (IS_NULL_OR_UNDEFINED(this) && !IS_UNDETECTABLE(this)) {
    throw MakeTypeError("called_on_null_or_undefined",
                        ["Array.prototype.splice"]);
  }

  var num_arguments = %_ArgumentsLength();

  var len = TO_UINT32(this.length);
  var start_i = TO_INTEGER(start);

  if (start_i < 0) {
    start_i += len;
    if (start_i < 0) start_i = 0;
  } else {
    if (start_i > len) start_i = len;
  }

  // SpiderMonkey, TraceMonkey and JSC treat the case where no delete count is
  // given as a request to delete all the elements from the start.
  // And it differs from the case of undefined delete count.
  // This does not follow ECMA-262, but we do the same for
  // compatibility.
  var del_count = 0;
  if (num_arguments == 1) {
    del_count = len - start_i;
  } else {
    del_count = TO_INTEGER(delete_count);
    if (del_count < 0) del_count = 0;
    if (del_count > len - start_i) del_count = len - start_i;
  }

  var deleted_elements = [];
  deleted_elements.length = del_count;

  // Number of elements to add.
  var num_additional_args = 0;
  if (num_arguments > 2) {
    num_additional_args = num_arguments - 2;
  }

  var use_simple_splice = true;

  if (IS_ARRAY(this) && num_additional_args !== del_count) {
    // If we are only deleting/moving a few things near the end of the
    // array then the simple version is going to be faster, because it
    // doesn't touch most of the array.
    var estimated_non_hole_elements = %EstimateNumberOfElements(this);
    if (len > 20 && (estimated_non_hole_elements >> 2) < (len - start_i)) {
      use_simple_splice = false;
    }
  }

  if (use_simple_splice) {
    SimpleSlice(this, start_i, del_count, len, deleted_elements);
    SimpleMove(this, start_i, del_count, len, num_additional_args);
  } else {
    SmartSlice(this, start_i, del_count, len, deleted_elements);
    SmartMove(this, start_i, del_count, len, num_additional_args);
  }

  // Insert the arguments into the resulting array in
  // place of the deleted elements.
  var i = start_i;
  var arguments_index = 2;
  var arguments_length = %_ArgumentsLength();
  while (arguments_index < arguments_length) {
    this[i++] = %_Arguments(arguments_index++);
  }
  this.length = len - del_count + num_additional_args;

  // Return the deleted elements.
  return deleted_elements;
}


function ArraySort(comparefn) {
  if (IS_NULL_OR_UNDEFINED(this) && !IS_UNDETECTABLE(this)) {
    throw MakeTypeError("called_on_null_or_undefined",
                        ["Array.prototype.sort"]);
  }

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

  var QuickSort = function QuickSort(a, from, to) {
    // Insertion sort is faster for short arrays.
    if (to - from <= 10) {
      InsertionSort(a, from, to);
      return;
    }
    // Find a pivot as the median of first, last and middle element.
    var v0 = a[from];
    var v1 = a[to - 1];
    var middle_index = from + ((to - from) >> 1);
    var v2 = a[middle_index];
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
    a[middle_index] = a[low_end];
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
    QuickSort(a, from, low_end);
    QuickSort(a, high_start, to);
  };

  // Copy elements in the range 0..length from obj's prototype chain
  // to obj itself, if obj has holes. Return one more than the maximal index
  // of a prototype property.
  var CopyFromPrototype = function CopyFromPrototype(obj, length) {
    var max = 0;
    for (var proto = obj.__proto__; proto; proto = proto.__proto__) {
      var indices = %GetArrayKeys(proto, length);
      if (indices.length > 0) {
        if (indices[0] == -1) {
          // It's an interval.
          var proto_length = indices[1];
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
    }
    return max;
  };

  // Set a value of "undefined" on all indices in the range from..to
  // where a prototype of obj has an element. I.e., shadow all prototype
  // elements in that range.
  var ShadowPrototypeElements = function(obj, from, to) {
    for (var proto = obj.__proto__; proto; proto = proto.__proto__) {
      var indices = %GetArrayKeys(proto, to);
      if (indices.length > 0) {
        if (indices[0] == -1) {
          // It's an interval.
          var proto_length = indices[1];
          for (var i = from; i < proto_length; i++) {
            if (proto.hasOwnProperty(i)) {
              obj[i] = void 0;
            }
          }
        } else {
          for (var i = 0; i < indices.length; i++) {
            var index = indices[i];
            if (!IS_UNDEFINED(index) && from <= index &&
                proto.hasOwnProperty(index)) {
              obj[index] = void 0;
            }
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
        obj[last_defined] = void 0;
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
      obj[i] = void 0;
    }
    for (i = length - num_holes; i < length; i++) {
      // For compatability with Webkit, do not expose elements in the prototype.
      if (i in obj.__proto__) {
        obj[i] = void 0;
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
    // local elements. This is not very efficient, but sorting with
    // inherited elements happens very, very rarely, if at all.
    // The specification allows "implementation dependent" behavior
    // if an element on the prototype chain has an element that
    // might interact with sorting.
    max_prototype_element = CopyFromPrototype(this, length);
  }

  var num_non_undefined = %RemoveArrayHoles(this, length);
  if (num_non_undefined == -1) {
    // There were indexed accessors in the array.  Move array holes and
    // undefineds to the end using a Javascript function that is safe
    // in the presence of accessors.
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
  if (IS_NULL_OR_UNDEFINED(this) && !IS_UNDETECTABLE(this)) {
    throw MakeTypeError("called_on_null_or_undefined",
                        ["Array.prototype.filter"]);
  }

  // Pull out the length so that modifications to the length in the
  // loop will not affect the looping and side effects are visible.
  var array = ToObject(this);
  var length = ToUint32(array.length);

  if (!IS_SPEC_FUNCTION(f)) {
    throw MakeTypeError('called_non_callable', [ f ]);
  }
  if (IS_NULL_OR_UNDEFINED(receiver)) {
    receiver = %GetDefaultReceiver(f) || receiver;
  } else if (!IS_SPEC_OBJECT(receiver)) {
    receiver = ToObject(receiver);
  }

  var result = new $Array();
  var accumulator = new InternalArray();
  var accumulator_length = 0;
  for (var i = 0; i < length; i++) {
    if (i in array) {
      var element = array[i];
      if (%_CallFunction(receiver, element, i, array, f)) {
        accumulator[accumulator_length++] = element;
      }
    }
  }
  %MoveArrayContents(accumulator, result);
  return result;
}


function ArrayForEach(f, receiver) {
  if (IS_NULL_OR_UNDEFINED(this) && !IS_UNDETECTABLE(this)) {
    throw MakeTypeError("called_on_null_or_undefined",
                        ["Array.prototype.forEach"]);
  }

  // Pull out the length so that modifications to the length in the
  // loop will not affect the looping and side effects are visible.
  var array = ToObject(this);
  var length = TO_UINT32(array.length);

  if (!IS_SPEC_FUNCTION(f)) {
    throw MakeTypeError('called_non_callable', [ f ]);
  }
  if (IS_NULL_OR_UNDEFINED(receiver)) {
    receiver = %GetDefaultReceiver(f) || receiver;
  } else if (!IS_SPEC_OBJECT(receiver)) {
    receiver = ToObject(receiver);
  }

  for (var i = 0; i < length; i++) {
    if (i in array) {
      var element = array[i];
      %_CallFunction(receiver, element, i, array, f);
    }
  }
}


// Executes the function once for each element present in the
// array until it finds one where callback returns true.
function ArraySome(f, receiver) {
  if (IS_NULL_OR_UNDEFINED(this) && !IS_UNDETECTABLE(this)) {
    throw MakeTypeError("called_on_null_or_undefined",
                        ["Array.prototype.some"]);
  }

  // Pull out the length so that modifications to the length in the
  // loop will not affect the looping and side effects are visible.
  var array = ToObject(this);
  var length = TO_UINT32(array.length);

  if (!IS_SPEC_FUNCTION(f)) {
    throw MakeTypeError('called_non_callable', [ f ]);
  }
  if (IS_NULL_OR_UNDEFINED(receiver)) {
    receiver = %GetDefaultReceiver(f) || receiver;
  } else if (!IS_SPEC_OBJECT(receiver)) {
    receiver = ToObject(receiver);
  }

  for (var i = 0; i < length; i++) {
    if (i in array) {
      var element = array[i];
      if (%_CallFunction(receiver, element, i, array, f)) return true;
    }
  }
  return false;
}


function ArrayEvery(f, receiver) {
  if (IS_NULL_OR_UNDEFINED(this) && !IS_UNDETECTABLE(this)) {
    throw MakeTypeError("called_on_null_or_undefined",
                        ["Array.prototype.every"]);
  }

  // Pull out the length so that modifications to the length in the
  // loop will not affect the looping and side effects are visible.
  var array = ToObject(this);
  var length = TO_UINT32(array.length);

  if (!IS_SPEC_FUNCTION(f)) {
    throw MakeTypeError('called_non_callable', [ f ]);
  }
  if (IS_NULL_OR_UNDEFINED(receiver)) {
    receiver = %GetDefaultReceiver(f) || receiver;
  } else if (!IS_SPEC_OBJECT(receiver)) {
    receiver = ToObject(receiver);
  }

  for (var i = 0; i < length; i++) {
    if (i in array) {
      var element = array[i];
      if (!%_CallFunction(receiver, element, i, array, f)) return false;
    }
  }
  return true;
}

function ArrayMap(f, receiver) {
  if (IS_NULL_OR_UNDEFINED(this) && !IS_UNDETECTABLE(this)) {
    throw MakeTypeError("called_on_null_or_undefined",
                        ["Array.prototype.map"]);
  }

  // Pull out the length so that modifications to the length in the
  // loop will not affect the looping and side effects are visible.
  var array = ToObject(this);
  var length = TO_UINT32(array.length);

  if (!IS_SPEC_FUNCTION(f)) {
    throw MakeTypeError('called_non_callable', [ f ]);
  }
  if (IS_NULL_OR_UNDEFINED(receiver)) {
    receiver = %GetDefaultReceiver(f) || receiver;
  } else if (!IS_SPEC_OBJECT(receiver)) {
    receiver = ToObject(receiver);
  }

  var result = new $Array();
  var accumulator = new InternalArray(length);
  for (var i = 0; i < length; i++) {
    if (i in array) {
      var element = array[i];
      accumulator[i] = %_CallFunction(receiver, element, i, array, f);
    }
  }
  %MoveArrayContents(accumulator, result);
  return result;
}


function ArrayIndexOf(element, index) {
  if (IS_NULL_OR_UNDEFINED(this) && !IS_UNDETECTABLE(this)) {
    throw MakeTypeError("called_on_null_or_undefined",
                        ["Array.prototype.indexOf"]);
  }

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
  if (UseSparseVariant(this, length, IS_ARRAY(this))) {
    var intervals = %GetArrayKeys(this, length);
    if (intervals.length == 2 && intervals[0] < 0) {
      // A single interval.
      var intervalMin = -(intervals[0] + 1);
      var intervalMax = intervalMin + intervals[1];
      if (min < intervalMin) min = intervalMin;
      max = intervalMax;  // Capped by length already.
      // Fall through to loop below.
    } else {
      if (intervals.length == 0) return -1;
      // Get all the keys in sorted order.
      var sortedKeys = GetSortedArrayKeys(this, intervals);
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
  if (IS_NULL_OR_UNDEFINED(this) && !IS_UNDETECTABLE(this)) {
    throw MakeTypeError("called_on_null_or_undefined",
                        ["Array.prototype.lastIndexOf"]);
  }

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
  if (UseSparseVariant(this, length, IS_ARRAY(this))) {
    var intervals = %GetArrayKeys(this, index + 1);
    if (intervals.length == 2 && intervals[0] < 0) {
      // A single interval.
      var intervalMin = -(intervals[0] + 1);
      var intervalMax = intervalMin + intervals[1];
      if (min < intervalMin) min = intervalMin;
      max = intervalMax;  // Capped by index already.
      // Fall through to loop below.
    } else {
      if (intervals.length == 0) return -1;
      // Get all the keys in sorted order.
      var sortedKeys = GetSortedArrayKeys(this, intervals);
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
  if (IS_NULL_OR_UNDEFINED(this) && !IS_UNDETECTABLE(this)) {
    throw MakeTypeError("called_on_null_or_undefined",
                        ["Array.prototype.reduce"]);
  }

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
  for (; i < length; i++) {
    if (i in array) {
      var element = array[i];
      current = %_CallFunction(receiver, current, element, i, array, callback);
    }
  }
  return current;
}

function ArrayReduceRight(callback, current) {
  if (IS_NULL_OR_UNDEFINED(this) && !IS_UNDETECTABLE(this)) {
    throw MakeTypeError("called_on_null_or_undefined",
                        ["Array.prototype.reduceRight"]);
  }

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
  for (; i >= 0; i--) {
    if (i in array) {
      var element = array[i];
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
  %SetProperty($Array.prototype, "constructor", $Array, DONT_ENUM);

  // Set up non-enumerable functions on the Array object.
  InstallFunctions($Array, DONT_ENUM, $Array(
    "isArray", ArrayIsArray
  ));

  var specialFunctions = %SpecialArrayFunctions({});

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
    "concat", getFunction("concat", ArrayConcat, 1),
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
    "join", getFunction("join", ArrayJoin),
    "pop", getFunction("pop", ArrayPop),
    "push", getFunction("push", ArrayPush)
  ));
}

SetUpArray();
