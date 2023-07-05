// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
(() => {

  createSuite('MixedFrom', 1000, MixedFrom, MixedFromSetup);
  createSuite(
      'MixedCowNoMapFrom', 1000, MixedCowNoMapFrom, MixedCowNoMapFromSetup);
  createSuite('MixedNonCowNoMapFrom', 1000, MixedNonCowNoMapFrom,
              MixedNonCowNoMapFromSetup);
  createSuite('SmiFrom', 1000, SmiFrom, SmiFromSetup);
  createSuite('SmallSmiFrom', 1000, SmallSmiFrom, SmallSmiFromSetup);
  createSuite('SmiCowNoMapFrom', 1000, SmiCowNoMapFrom, SmiCowNoMapFromSetup);
  createSuite(
      'SmiNonCowNoMapFrom', 1000, SmiNonCowNoMapFrom, SmiNonCowNoMapFromSetup);
  createSuite(
      'SmiNoIteratorFrom', 1000, SmiNoIteratorFrom, SmiNoIteratorFromSetup);
  createSuite(
      'TransplantedFrom', 1000, TransplantedFrom, TransplantedFromSetup);
  createSuite('DoubleFrom', 1000, DoubleFrom, DoubleFromSetup);
  createSuite('DoubleNoMapFrom', 1000, DoubleNoMapFrom, DoubleNoMapFromSetup);
  createSuite('StringFrom', 1000, StringFrom, StringFromSetup);
  createSuite(
      'StringCowNoMapFrom', 1000, StringCowNoMapFrom, StringCowNoMapFromSetup);
  createSuite('StringNonCowNoMapFrom', 1000, StringNonCowNoMapFrom,
              StringNonCowNoMapFromSetup);

  function ArrayLike() {}
  ArrayLike.from = Array.from;

  var arg
  var result;
  var func

  // This creates a COW array of smis. COWness does not affect the performance
  // of Array.from calls with a callback function.
  var smi_array_Cow = [
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
  ];

  // This creates a non-COW array.
  var smi_array = Array.from(smi_array_Cow);
  smi_array[0] = 1;

  // This creates an array of doubles. There is no COW array for doubles.
  var double_array = [
    1.5,  2.5,  3.5,  4.5,  5.5,  6.5,  7.5,  8.5,  9.5,  10.5,  //
    11.5, 12.5, 13.5, 14.5, 15.5, 16.5, 17.5, 18.5, 19.5, 20.5,
    1.5,  2.5,  3.5,  4.5,  5.5,  6.5,  7.5,  8.5,  9.5,  10.5,  //
    11.5, 12.5, 13.5, 14.5, 15.5, 16.5, 17.5, 18.5, 19.5, 20.5,
    1.5,  2.5,  3.5,  4.5,  5.5,  6.5,  7.5,  8.5,  9.5,  10.5,  //
    11.5, 12.5, 13.5, 14.5, 15.5, 16.5, 17.5, 18.5, 19.5, 20.5,
    1.5,  2.5,  3.5,  4.5,  5.5,  6.5,  7.5,  8.5,  9.5,  10.5,  //
    11.5, 12.5, 13.5, 14.5, 15.5, 16.5, 17.5, 18.5, 19.5, 20.5,
    1.5,  2.5,  3.5,  4.5,  5.5,  6.5,  7.5,  8.5,  9.5,  10.5,  //
    11.5, 12.5, 13.5, 14.5, 15.5, 16.5, 17.5, 18.5, 19.5, 20.5,
  ];

  // This creates a COW array of objects.
  var string_array_Cow = [
    'a', 'b', 'c', 'a', 'b', 'c', 'a', 'b', 'c', 'a', 'a', 'b', 'c', 'a', 'b',
    'c', 'a', 'b', 'c', 'a', 'a', 'b', 'c', 'a', 'b', 'c', 'a', 'b', 'c', 'a',
    'a', 'b', 'c', 'a', 'b', 'c', 'a', 'b', 'c', 'a', 'a', 'b', 'c', 'a', 'b',
    'c', 'a', 'b', 'c', 'a', 'a', 'b', 'c', 'a', 'b', 'c', 'a', 'b', 'c', 'a',
    'a', 'b', 'c', 'a', 'b', 'c', 'a', 'b', 'c', 'a', 'a', 'b', 'c', 'a', 'b',
    'c', 'a', 'b', 'c', 'a', 'a', 'b', 'c', 'a', 'b', 'c', 'a', 'b', 'c', 'a',
    'a', 'b', 'c', 'a', 'b', 'c', 'a', 'b', 'c', 'a',
  ];

  // This creates a non-COW array.
  var string_array = Array.from(string_array_Cow);
  string_array[0] = 'a';

  // This creates a COW array of objects.
  var mixed_array_Cow = [
    1,    2,    3,    4,    5,    6,    7,    8,    9,    10,
    11,   12,   13,   14,   15,   16,   17,   18,   19,   20,  //
    1,    2,    3,    4,    5,    6,    7,    8,    9,    10,
    11,   12,   13,   14,   15,   16,   17,   18,   19,   20,  //
    1.5,  2.5,  3.5,  4.5,  5.5,  6.5,  7.5,  8.5,  9.5,  10.5,
    11.5, 12.5, 13.5, 14.5, 15.5, 16.5, 17.5, 18.5, 19.5, 20.5,  //
    1.5,  2.5,  3.5,  4.5,  5.5,  6.5,  7.5,  8.5,  9.5,  10.5,
    11.5, 12.5, 13.5, 14.5, 15.5, 16.5, 17.5, 18.5, 19.5, 20.5,  //
    'a',  'b',  'c',  'a',  'b',  'c',  'a',  'b',  'c',  'a',
    'a',  'b',  'c',  'a',  'b',  'c',  'a',  'b',  'c',  'a',
  ];

  // This creates a non-COW array.
  var mixed_array = Array.from(mixed_array_Cow);
  mixed_array[0] = 1;

  // Although these functions have the same code, they are separated for
  // clean IC feedback.
  function SmallSmiFrom() {
    result = Array.from(arg, func);
  }

  function SmiCowNoMapFrom() {
    result = Array.from(arg);
  }

  function SmiNonCowNoMapFrom() {
    result = Array.from(arg);
  }

  function SmiFrom() {
    result = Array.from(arg, func);
  }

  function SmiNoIteratorFrom() {
    result = Array.from(arg, func);
  }

  function TransplantedFrom() {
    result = ArrayLike.from(arg, func);
  }

  function DoubleFrom() {
    result = Array.from(arg, func);
  }

  function DoubleNoMapFrom() {
    result = Array.from(arg);
  }

  function StringFrom() {
    result = Array.from(arg, func);
  }

  function StringCowNoMapFrom() {
    result = Array.from(arg);
  }

  function StringNonCowNoMapFrom() {
    result = Array.from(arg);
  }

  function MixedFrom() {
    result = Array.from(arg, func);
  }

  function MixedCowNoMapFrom() {
    result = Array.from(arg);
  }

  function MixedNonCowNoMapFrom() {
    result = Array.from(arg);
  }

  function SmallSmiFromSetup() {
    func = (v, i) => v + i;
    arg = [1, 2, 3];
  }

  function SmiCowNoMapFromSetup() {
    func = undefined;
    arg = smi_array_Cow;
  }

  function SmiNonCowNoMapFromSetup() {
    func = undefined;
    arg = smi_array;
  }

  function SmiFromSetup() {
    func = (v, i) => v + i;
    arg = smi_array_Cow;
  }

  function SmiNoIteratorFromSetup() {
    func = (v, i) => v + i;
    array = smi_array_Cow;
    arg = {length: array.length};
    Object.assign(arg, array);
  }

  function TransplantedFromSetup() {
    func = (v, i) => v + i;
    arg = smi_array_Cow;
  }

  function DoubleFromSetup() {
    func = (v, i) => v + i;
    arg = double_array;
  }

  function DoubleNoMapFromSetup() {
    func = undefined;
    arg = double_array;
  }

  function StringFromSetup() {
    func = (v, i) => v + i;
    arg = string_array_Cow;
  }

  function StringCowNoMapFromSetup() {
    func = undefined;
    arg = string_array_Cow;
  }

  function StringNonCowNoMapFromSetup() {
    func = undefined;
    arg = string_array;
  }

  function MixedFromSetup() {
    func = (v, i) => v + i;
    arg = mixed_array_Cow;
  }

  function MixedCowNoMapFromSetup() {
    func = undefined;
    arg = mixed_array_Cow;
  }

  function MixedNonCowNoMapFromSetup() {
    func = undefined;
    arg = mixed_array;
  }
})();
