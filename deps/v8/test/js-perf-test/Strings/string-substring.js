// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function benchy (f, name) {
  new BenchmarkSuite(name, [5], [
    new Benchmark(name, true, false, 0,
    f),
  ]);
}

const subjects = ['abcde', '123456', 'aqwsde', "nbvveqxu", "f03ks-120-3;jfkm;ajp3f", "sd-93u498thikefnow8y3-0rh1nalksfnwo8y3t19-3r8hoiefnw"];

// Drop first element.

function StringDropFirstSlice() {
  var sum = "";

  for (var j = 0; j < subjects.length; ++j) {
    let s = subjects[j];
    sum += s.slice(1);
  }

  return sum;
}
benchy(StringDropFirstSlice, "StringDropFirstSlice");

function StringDropFirstSubstr() {
  var sum = "";

  for (var j = 0; j < subjects.length; ++j) {
    let s = subjects[j];
    sum += s.substr(1);
  }

  return sum;
}
benchy(StringDropFirstSubstr, "StringDropFirstSubstr");

function StringDropFirstSubstring() {
  var sum = "";

  for (var j = 0; j < subjects.length; ++j) {
    let s = subjects[j];
    sum += s.substring(1);
  }

  return sum;
}
benchy(StringDropFirstSubstring, "StringDropFirstSubstring");


// Take first element.

function StringTakeFirstSlice() {
  var sum = "";

  for (var j = 0; j < subjects.length; ++j) {
    let s = subjects[j];
    sum += s.slice(0,1);
  }

  return sum;
}
benchy(StringTakeFirstSlice, "StringTakeFirstSlice");

function StringTakeFirstSubstr() {
  var sum = "";

  for (var j = 0; j < subjects.length; ++j) {
    let s = subjects[j];
    sum += s.substr(0,1);
  }

  return sum;
}
benchy(StringTakeFirstSubstr, "StringTakeFirstSubstr");

function StringTakeFirstSubstring() {
  var sum = "";

  for (var j = 0; j < subjects.length; ++j) {
    let s = subjects[j];
    sum += s.substring(0, 1);
  }

  return sum;
}
benchy(StringTakeFirstSubstring, "StringTakeFirstSubstring");


// Drop last element.

function StringDropLastSlice() {
  var sum = "";

  for (var j = 0; j < subjects.length; ++j) {
    let s = subjects[j];
    sum += s.slice(0, -1);
  }

  return sum;
}
benchy(StringDropLastSlice, "StringDropLastSlice");

function StringDropLastSubstr() {
  var sum = "";

  for (var j = 0; j < subjects.length; ++j) {
    let s = subjects[j];
    sum += s.substr(0, s.length-1);
  }

  return sum;
}
benchy(StringDropLastSubstr, "StringDropLastSubstr");

function StringDropLastSubstring() {
  var sum = "";

  for (var j = 0; j < subjects.length; ++j) {
    let s = subjects[j];
    sum += s.substring(0, s.length-1);
  }

  return sum;
}
benchy(StringDropLastSubstring, "StringDropLastSubstring");


// Take last element.

function StringTakeLastSlice() {
  var sum = "";

  for (var j = 0; j < subjects.length; ++j) {
    let s = subjects[j];
    sum += s.slice(-1);
  }

  return sum;
}
benchy(StringTakeLastSlice, "StringTakeLastSlice");

function StringTakeLastSubstr() {
  var sum = "";

  for (var j = 0; j < subjects.length; ++j) {
    let s = subjects[j];
    sum += s.substr(-1);
  }

  return sum;
}
benchy(StringTakeLastSubstr, "StringTakeLastSubstr");

function StringTakeLastSubstring() {
  var sum = "";

  for (var j = 0; j < subjects.length; ++j) {
    let s = subjects[j];
    sum += s.substring(s.length-1, s.length);
  }

  return sum;
}
benchy(StringTakeLastSubstring, "StringTakeLastSubstring");


// Drop first and last.

function StringDropFirstSlice() {
  var sum = "";

  for (var j = 0; j < subjects.length; ++j) {
    let s = subjects[j];
    sum += s.slice(1, -1);
  }

  return sum;
}
benchy(StringDropFirstSlice, "StringDropFirstSlice");

function StringDropFirstSubstr() {
  var sum = "";

  for (var j = 0; j < subjects.length; ++j) {
    let s = subjects[j]
    sum += s.substr(1, s.length-2);
  }

  return sum;
}
benchy(StringDropFirstSubstr, "StringDropFirstSubstr");

function StringDropFirstSubstring() {
  var sum = "";

  for (var j = 0; j < subjects.length; ++j) {
    let s = subjects[j];
    sum += s.substring(1, s.length-1);
  }

  return sum;
}
benchy(StringDropFirstSubstring, "StringDropFirstSubstring");
