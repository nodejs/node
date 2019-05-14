// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function SimpleCtor() {
  new RegExp("[Cz]");
}

function FlagsCtor() {
  new RegExp("[Cz]", "guiym");
}

function SimpleCtorWithoutNew() {
  RegExp("[Cz]");
}

function FlagsCtorWithoutNew() {
  RegExp("[Cz]", "guiym");
}

function CtorWithRegExpPattern() {
  new RegExp(/[Cz]/);
}

function CtorWithRegExpPatternAndFlags() {
  new RegExp(/[Cz]/, "guiym");
}

class SubRegExp extends RegExp {
  get source() { return "[Cz]"; }
  get flags() { return "guiym"; }
}

function CtorWithRegExpSubclassPattern() {
  new RegExp(new SubRegExp(/[Cz]/));
}

function CtorWithUndefinedPattern() {
  new RegExp();
}

function CtorWithFlagsAndUndefinedPattern() {
  new RegExp(undefined, "guiym");
}

var benchmarks = [ [SimpleCtor, undefined],
                   [FlagsCtor, undefined],
                   [SimpleCtorWithoutNew, undefined],
                   [FlagsCtorWithoutNew, undefined],
                   [CtorWithRegExpPattern, undefined],
                   [CtorWithRegExpPatternAndFlags, undefined],
                   [CtorWithRegExpSubclassPattern, undefined],
                   [CtorWithUndefinedPattern, undefined],
                   [CtorWithFlagsAndUndefinedPattern, undefined],
                 ];
