// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

new BenchmarkSuite('Untagged', [1000], [
  new Benchmark('Untagged-Simple', false, false, 0,
                Untagged, UntaggedSetup, UntaggedTearDown),
]);

new BenchmarkSuite('LargeUntagged', [1000], [
  new Benchmark('Untagged-Large', false, false, 0,
                UntaggedLarge, UntaggedLargeSetup, UntaggedLargeTearDown),
]);

new BenchmarkSuite('Tagged', [1000], [
  new Benchmark('TaggedRawSimple', false, false, 0,
                TaggedRaw, TaggedRawSetup, TaggedRawTearDown),
]);

var result;
var SUBJECT = 'Bob';
var TARGET = 'Mary';
var OBJECT = 'apple';

function UntaggedSetup() {
  result = undefined;
}

function Untagged() {
  result = `${SUBJECT} gives ${TARGET} an ${OBJECT}.`;
}

function UntaggedTearDown() {
  var expected = '' + SUBJECT + ' gives ' + TARGET + ' an ' + OBJECT + '.';
  return result === expected;
}


// ----------------------------------------------------------------------------

function UntaggedLargeSetup() {
  result = undefined;
}

function UntaggedLarge() {
  result = `Lorem ipsum dolor sit amet, consectetur adipiscing elit. Vivamus
 aliquam, elit euismod vestibulum ${0}lacinia, arcu odio sagittis mauris, id
 blandit dolor felis pretium nisl. Maecenas porttitor, nunc ut accumsan mollis,
 arcu metus rutrum arcu, ${1}ut varius dolor lorem nec risus. Integer convallis
 tristique ante, non pretium ante suscipit at. Sed egestas massa enim, convallis
 fermentum neque vehicula ac. Donec imperdiet a tortor ac semper. Morbi accumsan
 quam nec erat viverra iaculis. ${2}Donec a scelerisque cras amet.`;
}

function UntaggedLargeTearDown() {
  var expected = "Lorem ipsum dolor sit amet, consectetur adipiscing elit. " +
      "Vivamus\n aliquam, elit euismod vestibulum " + 0 + "lacinia, arcu odio" +
      " sagittis mauris, id\n blandit dolor felis pretium nisl. Maecenas " +
      "porttitor, nunc ut accumsan mollis,\n arcu metus rutrum arcu, " + 1 +
      "ut varius dolor lorem nec risus. Integer convallis\n tristique ante, " +
      "non pretium ante suscipit at. Sed egestas massa enim, convallis\n " +
      "fermentum neque vehicula ac. Donec imperdiet a tortor ac semper. Morbi" +
      " accumsan\n quam nec erat viverra iaculis. " + 2 + "Donec a " +
      "scelerisque cras amet.";
  return result === expected;
}


// ----------------------------------------------------------------------------


function TaggedRawSetup() {
  result = undefined;
}

function TaggedRaw() {
  result = String.raw `${SUBJECT} gives ${TARGET} an ${OBJECT} \ud83c\udf4f.`;
}

function TaggedRawTearDown() {
  var expected =
      '' + SUBJECT + ' gives ' + TARGET + ' an ' + OBJECT + ' \\ud83c\\udf4f.';
  return result === expected;
}


// ----------------------------------------------------------------------------
