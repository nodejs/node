// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

Debug = debug.Debug

eval("var something1 = 25; "
     + "function do_eval(s) { return eval(`'${s}'`) };"
     + " function ChooseAnimal() { return do_eval('Cat'); } "
     + " ChooseAnimal.Helper = function() { return eval(\"'Help!\"); }");

assertEquals("Cat", ChooseAnimal());

var orig_animal = "Cat";
var new_animal_patch = "Cap' + 'y' + 'bara";
var old_source = Debug.scriptSource(ChooseAnimal);
var new_source = old_source.replace(orig_animal, new_animal_patch);

%LiveEditPatchScript(ChooseAnimal, new_source);

assertEquals("Capybara", ChooseAnimal());
