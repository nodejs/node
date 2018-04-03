// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file

// Flags: --allow-natives-syntax --enable-inspector

Debug = debug.Debug

eval("var something1 = 25; "
     + " function ChooseAnimal() { return          'Cat';          } "
     + " ChooseAnimal.Helper = function() { return 'Help!'; }");

function foo() {  return ChooseAnimal() }

assertEquals("Cat", foo());
    %OptimizeFunctionOnNextCall(foo);

foo();

var script = Debug.findScript(ChooseAnimal);

var orig_animal = "Cat";
var patch_pos = script.source.indexOf(orig_animal);
var new_animal_patch = "Cap' + 'y' + 'bara";

var change_log = new Array();

Debug.LiveEdit.TestApi.ApplySingleChunkPatch(script, patch_pos, orig_animal.length, new_animal_patch, change_log);

assertEquals("Capybara", foo());
