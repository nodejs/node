// Copyright 2006-2008 the V8 project authors. All rights reserved.
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
// const $Array = global.Array;

// -------------------------------------------------------------------

// Global list of arrays visited during toString, toLocaleString and
// join invocations.
var visited_arrays = new $Array();


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


// Optimized for sparse arrays if separator is ''.
function SparseJoin(array, len, convert) {
  var keys = GetSortedArrayKeys(array, %GetArrayKeys(array, len));
  var last_key = -1;
  var keys_length = keys.length;

  var elements = new $Array(keys_length);
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
    if (UseSparseVariant(array, length, is_array) && (separator.length == 0)) {
      return SparseJoin(array, length, convert);
    }

    // Fast case for one-element arrays.
    if (length == 1) {
      var e = array[0];
      if (!IS_UNDEFINED(e) || (0 in array)) {
        if (IS_STRING(e)) return e;
        return convert(e);
      }
    }

    // Construct an array for the elements.
    var elements;
    var elements_length = 0;

    // We pull the empty separator check outside the loop for speed!
    if (separator.length == 0) {
      elements = new $Array(length);
      for (var i = 0; i < length; i++) {
        var e = array[i];
        if (!IS_UNDEFINED(e) || (i in array)) {
          if (!IS_STRING(e)) e = convert(e);
          elements[elements_length++] = e;
        }
      }
    } else {
      elements = new $Array(length << 1);
      for (var i = 0; i < length; i++) {
        var e = array[i];
        if (i != 0) elements[elements_length++] = separator;
        if (!IS_UNDEFINED(e) || (i in array)) {
          if (!IS_STRING(e)) e = convert(e);
          elements[elements_length++] = e;
        }
      }
    }
    return %StringBuilderConcat(elements, elements_length, '');
  } finally {
    // Make sure to pop the visited array no matter what happens.
    if (is_array) visited_arrays.pop();
  }
}


function ConvertToString(e) {
  if (e == null) return '';
  else return ToString(e);
}


function ConvertToLocaleString(e) {
  if (e == null) {
    return '';
  } else {
    // e_obj's toLocaleString might be overwritten, check if it is a function.
    // Call ToString if toLocaleString is not a function.
    // See issue 877615.
    var e_obj = ToObject(e);
    if (IS_FUNCTION(e_obj.toLocaleString))
      return ToString(e_obj.toLocaleString());
    else
      return ToString(e);
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
  var new_array = new $Array(len - del_count + num_additional_args);
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
    if (!IS_UNDEFINED(current) || index in array)
      deleted_elements[i] = current;
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
  if (!IS_ARRAY(this)) {
    throw new $TypeError('Array.prototype.toString is not generic');
  }
  return Join(this, this.length, ',', ConvertToString);
}


function ArrayToLocaleString() {
  if (!IS_ARRAY(this)) {
    throw new $TypeError('Array.prototype.toString is not generic');
  }
  return Join(this, this.length, ',', ConvertToLocaleString);
}


function ArrayJoin(separator) {
  if (IS_UNDEFINED(separator)) {
    separator = ',';
  } else if (!IS_STRING(separator)) {
    separator = ToString(separator);
  }
  var length = TO_UINT32(this.length);
  return Join(this, length, separator, ConvertToString);
}


// Removes the last element from the array and returns it. See
// ECMA-262, section 15.4.4.6.
function ArrayPop() {
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
  var n = TO_UINT32(this.length);
  var m = %_ArgumentsLength();
  for (var i = 0; i < m; i++) {
    this[i+n] = %_Arguments(i);
  }
  this.length = n + m;
  return this.length;
}


function ArrayConcat(arg1) {  // length == 1
  // TODO: can we just use arguments?
  var arg_count = %_ArgumentsLength();
  var arrays = new $Array(1 + arg_count);
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
      while (keys[--high_counter] == j);
      low = j_complement;
    }
    if (j_complement >= i) {
      low = i;
      while (keys[++low_counter] == i);
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
  var len = TO_UINT32(this.length);

  if (len === 0) {
    this.length = 0;
    return;
  }

  var first = this[0];

  if (IS_ARRAY(this))
    SmartMove(this, 0, 1, len, 0);
  else
    SimpleMove(this, 0, 1, len, 0);

  this.length = len - 1;

  return first;
}


function ArrayUnshift(arg1) {  // length == 1
  var len = TO_UINT32(this.length);
  var num_arguments = %_ArgumentsLength();

  if (IS_ARRAY(this))
    SmartMove(this, 0, 0, len, num_arguments);
  else
    SimpleMove(this, 0, 0, len, num_arguments);

  for (var i = 0; i < num_arguments; i++) {
    this[i] = %_Arguments(i);
  }

  this.length = len + num_arguments;

  return len + num_arguments;
}


function ArraySlice(start, end) {
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

  if (IS_ARRAY(this)) {
    SmartSlice(this, start_i, end_i - start_i, len, result);
  } else {
    SimpleSlice(this, start_i, end_i - start_i, len, result);
  }

  result.length = end_i - start_i;

  return result;
}


function ArraySplice(start, delete_count) {
  var num_arguments = %_ArgumentsLength();

  // SpiderMonkey and JSC return undefined in the case where no
  // arguments are given instead of using the implicit undefined
  // arguments.  This does not follow ECMA-262, but we do the same for
  // compatibility.
  // TraceMonkey follows ECMA-262 though.
  if (num_arguments == 0) return;

  var len = TO_UINT32(this.length);
  var start_i = TO_INTEGER(start);

  if (start_i < 0) {
    start_i += len;
    if (start_i < 0) start_i = 0;
  } else {
    if (start_i > len) start_i = len;
  }

  // SpiderMonkey, TraceMonkey and JSC treat the case where no delete count is
  // given differently from when an undefined delete count is given.
  // This does not follow ECMA-262, but we do the same for
  // compatibility.
  var del_count = 0;
  if (num_arguments > 1) {
    del_count = TO_INTEGER(delete_count);
    if (del_count < 0) del_count = 0;
    if (del_count > len - start_i) del_count = len - start_i;
  } else {
    del_count = len - start_i;
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
  // In-place QuickSort algorithm.
  // For short (length <= 22) arrays, insertion sort is used for efficiency.

  if (!IS_FUNCTION(comparefn)) {
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
  var global_receiver = %GetGlobalReceiver();

  function InsertionSort(a, from, to) {
    for (var i = from + 1; i < to; i++) {
      var element = a[i];
      for (var j = i - 1; j >= from; j--) {
        var tmp = a[j];
        var order = %_CallFunction(global_receiver, tmp, element, comparefn);
        if (order > 0) {
          a[j + 1] = tmp;
        } else {
          break;
        }
      }
      a[j + 1] = element;
    }
  }

  function QuickSort(a, from, to) {
    // Insertion sort is faster for short arrays.
    if (to - from <= 22) {
      InsertionSort(a, from, to);
      return;
    }
    var pivot_index = $floor($random() * (to - from)) + from;
    var pivot = a[pivot_index];
    // Issue 95: Keep the pivot element out of the comparisons to avoid
    // infinite recursion if comparefn(pivot, pivot) != 0.
    a[pivot_index] = a[from];
    a[from] = pivot;
    var low_end = from;   // Upper bound of the elements lower than pivot.
    var high_start = to;  // Lower bound of the elements greater than pivot.
    // From low_end to i are elements equal to pivot.
    // From i to high_start are elements that haven't been compared yet.
    for (var i = from + 1; i < high_start; ) {
      var element = a[i];
      var order = %_CallFunction(global_receiver, element, pivot, comparefn);
      if (order < 0) {
        a[i] = a[low_end];
        a[low_end] = element;
        i++;
        low_end++;
      } else if (order > 0) {
        high_start--;
        a[i] = a[high_start];
        a[high_start] = element;
      } else {  // order == 0
        i++;
      }
    }
    QuickSort(a, from, low_end);
    QuickSort(a, high_start, to);
  }

  // Copies elements in the range 0..length from obj's prototype chain
  // to obj itself, if obj has holes. Returns one more than the maximal index
  // of a prototype property.
  function CopyFromPrototype(obj, length) {
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
  }

  // Set a value of "undefined" on all indices in the range from..to
  // where a prototype of obj has an element. I.e., shadow all prototype
  // elements in that range.
  function ShadowPrototypeElements(obj, from, to) {
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
  }

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
  }

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
  if (!IS_FUNCTION(f)) {
    throw MakeTypeError('called_non_callable', [ f ]);
  }
  // Pull out the length so that modifications to the length in the
  // loop will not affect the looping.
  var length = this.length;
  var result = [];
  var result_length = 0;
  for (var i = 0; i < length; i++) {
    var current = this[i];
    if (!IS_UNDEFINED(current) || i in this) {
      if (f.call(receiver, current, i, this)) result[result_length++] = current;
    }
  }
  return result;
}


function ArrayForEach(f, receiver) {
  if (!IS_FUNCTION(f)) {
    throw MakeTypeError('called_non_callable', [ f ]);
  }
  // Pull out the length so that modifications to the length in the
  // loop will not affect the looping.
  var length =  TO_UINT32(this.length);
  for (var i = 0; i < length; i++) {
    var current = this[i];
    if (!IS_UNDEFINED(current) || i in this) {
      f.call(receiver, current, i, this);
    }
  }
}


// Executes the function once for each element present in the
// array until it finds one where callback returns true.
function ArraySome(f, receiver) {
  if (!IS_FUNCTION(f)) {
    throw MakeTypeError('called_non_callable', [ f ]);
  }
  // Pull out the length so that modifications to the length in the
  // loop will not affect the looping.
  var length = TO_UINT32(this.length);
  for (var i = 0; i < length; i++) {
    var current = this[i];
    if (!IS_UNDEFINED(current) || i in this) {
      if (f.call(receiver, current, i, this)) return true;
    }
  }
  return false;
}


function ArrayEvery(f, receiver) {
  if (!IS_FUNCTION(f)) {
    throw MakeTypeError('called_non_callable', [ f ]);
  }
  // Pull out the length so that modifications to the length in the
  // loop will not affect the looping.
  var length = TO_UINT32(this.length);
  for (var i = 0; i < length; i++) {
    var current = this[i];
    if (!IS_UNDEFINED(current) || i in this) {
      if (!f.call(receiver, current, i, this)) return false;
    }
  }
  return true;
}

function ArrayMap(f, receiver) {
  if (!IS_FUNCTION(f)) {
    throw MakeTypeError('called_non_callable', [ f ]);
  }
  // Pull out the length so that modifications to the length in the
  // loop will not affect the looping.
  var length = TO_UINT32(this.length);
  var result = new $Array(length);
  for (var i = 0; i < length; i++) {
    var current = this[i];
    if (!IS_UNDEFINED(current) || i in this) {
      result[i] = f.call(receiver, current, i, this);
    }
  }
  return result;
}


function ArrayIndexOf(element, index) {
  var length = this.length;
  if (index == null) {
    index = 0;
  } else {
    index = TO_INTEGER(index);
    // If index is negative, index from the end of the array.
    if (index < 0) index = length + index;
    // If index is still negative, search the entire array.
    if (index < 0) index = 0;
  }
  if (!IS_UNDEFINED(element)) {
    for (var i = index; i < length; i++) {
      if (this[i] === element) return i;
    }
    return -1;
  }
  // Lookup through the array.
  for (var i = index; i < length; i++) {
    if (IS_UNDEFINED(this[i]) && i in this) {
      return i;
    }
  }
  return -1;
}


function ArrayLastIndexOf(element, index) {
  var length = this.length;
  if (index == null) {
    index = length - 1;
  } else {
    index = TO_INTEGER(index);
    // If index is negative, index from end of the array.
    if (index < 0) index = length + index;
    // If index is still negative, do not search the array.
    if (index < 0) index = -1;
    else if (index >= length) index = length - 1;
  }
  // Lookup through the array.
  if (!IS_UNDEFINED(element)) {
    for (var i = index; i >= 0; i--) {
      if (this[i] === element) return i;
    }
    return -1;
  }
  for (var i = index; i >= 0; i--) {
    if (IS_UNDEFINED(this[i]) && i in this) {
      return i;
    }
  }
  return -1;
}


function ArrayReduce(callback, current) {
  if (!IS_FUNCTION(callback)) {
    throw MakeTypeError('called_non_callable', [callback]);
  }
  // Pull out the length so that modifications to the length in the
  // loop will not affect the looping.
  var length = this.length;
  var i = 0;

  find_initial: if (%_ArgumentsLength() < 2) {
    for (; i < length; i++) {
      current = this[i];
      if (!IS_UNDEFINED(current) || i in this) {
        i++;
        break find_initial;
      }
    }
    throw MakeTypeError('reduce_no_initial', []);
  }

  for (; i < length; i++) {
    var element = this[i];
    if (!IS_UNDEFINED(element) || i in this) {
      current = callback.call(null, current, element, i, this);
    }
  }
  return current;
}

function ArrayReduceRight(callback, current) {
  if (!IS_FUNCTION(callback)) {
    throw MakeTypeError('called_non_callable', [callback]);
  }
  var i = this.length - 1;

  find_initial: if (%_ArgumentsLength() < 2) {
    for (; i >= 0; i--) {
      current = this[i];
      if (!IS_UNDEFINED(current) || i in this) {
        i--;
        break find_initial;
      }
    }
    throw MakeTypeError('reduce_no_initial', []);
  }

  for (; i >= 0; i--) {
    var element = this[i];
    if (!IS_UNDEFINED(element) || i in this) {
      current = callback.call(null, current, element, i, this);
    }
  }
  return current;
}

// ES5, 15.4.3.2
function ArrayIsArray(obj) {
  return IS_ARRAY(obj);
}


// -------------------------------------------------------------------
function SetupArray() {
  // Setup non-enumerable constructor property on the Array.prototype
  // object.
  %SetProperty($Array.prototype, "constructor", $Array, DONT_ENUM);

  // Setup non-enumerable functions on the Array object.
  InstallFunctions($Array, DONT_ENUM, $Array(
    "isArray", ArrayIsArray
  ));

  var specialFunctions = %SpecialArrayFunctions({});

  function getFunction(name, jsBuiltin, len) {
    var f = jsBuiltin;
    if (specialFunctions.hasOwnProperty(name)) {
      f = specialFunctions[name];
    }
    if (!IS_UNDEFINED(len)) {
      %FunctionSetLength(f, len);
    }
    return f;
  }

  // Setup non-enumerable functions of the Array.prototype object and
  // set their names.
  // Manipulate the length of some of the functions to meet
  // expectations set by ECMA-262 or Mozilla.
  InstallFunctionsOnHiddenPrototype($Array.prototype, DONT_ENUM, $Array(
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
}


SetupArray();
