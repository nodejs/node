// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
(() => {

  createSuite('MixedFrom', 1000, MixedFrom, MixedFromSetup);
  createSuite('SmiFrom', 1000, SmiFrom, SmiFromSetup);
  createSuite('SmallSmiFrom', 1000, SmallSmiFrom, SmallSmiFromSetup);
  createSuite('SmiNoMapFrom', 1000, SmiNoMapFrom, SmiNoMapFromSetup);
  createSuite(
      'SmiNoIteratorFrom', 1000, SmiNoIteratorFrom, SmiNoIteratorFromSetup);
  createSuite(
      'TransplantedFrom', 1000, TransplantedFrom, TransplantedFromSetup);
  createSuite('DoubleFrom', 1000, DoubleFrom, DoubleFromSetup);
  createSuite('StringFrom', 1000, StringFrom, StringFromSetup);
  createSuite('StringNoMapFrom', 1000, StringNoMapFrom, StringNoMapFromSetup);

  function ArrayLike() {}
  ArrayLike.from = Array.from;

  var arg
  var result;
  var func

  var smi_array = [
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
  ];

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

  var string_array = [
    'a', 'b', 'c', 'a', 'b', 'c', 'a', 'b', 'c', 'a', 'a', 'b', 'c', 'a', 'b',
    'c', 'a', 'b', 'c', 'a', 'a', 'b', 'c', 'a', 'b', 'c', 'a', 'b', 'c', 'a',
    'a', 'b', 'c', 'a', 'b', 'c', 'a', 'b', 'c', 'a', 'a', 'b', 'c', 'a', 'b',
    'c', 'a', 'b', 'c', 'a', 'a', 'b', 'c', 'a', 'b', 'c', 'a', 'b', 'c', 'a',
    'a', 'b', 'c', 'a', 'b', 'c', 'a', 'b', 'c', 'a', 'a', 'b', 'c', 'a', 'b',
    'c', 'a', 'b', 'c', 'a', 'a', 'b', 'c', 'a', 'b', 'c', 'a', 'b', 'c', 'a',
    'a', 'b', 'c', 'a', 'b', 'c', 'a', 'b', 'c', 'a',
  ];

  var mixed_array = [
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

  // Although these functions have the same code, they are separated for
  // clean IC feedback.
  function SmallSmiFrom() {
    result = Array.from(arg, func);
  }

  function SmiNoMapFrom() {
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

  function StringFrom() {
    result = Array.from(arg, func);
  }

  function StringNoMapFrom() {
    result = Array.from(arg);
  }

  function MixedFrom() {
    result = Array.from(arg, func);
  }

  function SmallSmiFromSetup() {
    func = (v, i) => v + i;
    arg = [1, 2, 3];
  }

  function SmiNoMapFromSetup() {
    func = undefined;
    arg = smi_array;
  }

  function SmiFromSetup() {
    func = (v, i) => v + i;
    arg = smi_array;
  }

  function SmiNoIteratorFromSetup() {
    func = (v, i) => v + i;
    array = smi_array;
    arg = {length: array.length};
    Object.assign(arg, array);
  }

  function TransplantedFromSetup() {
    func = (v, i) => v + i;
    arg = smi_array;
  }

  function DoubleFromSetup() {
    func = (v, i) => v + i;
    arg = double_array;
  }

  function StringFromSetup() {
    func = (v, i) => v + i;
    arg = string_array;
  }

  function StringNoMapFromSetup() {
    func = undefined;
    arg = string_array;
  }

  function MixedFromSetup() {
    func = (v, i) => v + i;
    arg = mixed_array;
  }

})();
