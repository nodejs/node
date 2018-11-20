// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file

// Flags: --allow-natives-syntax

Debug = debug.Debug

eval("var something1 = 25; "
     + " function ChooseAnimal() { return          'Cat';          } "
     + " ChooseAnimal.Helper = function() { return 'Help!'; }");

function foo() {  return ChooseAnimal() }

assertEquals("Cat", foo());
    %OptimizeFunctionOnNextCall(foo);

foo();

var new_source =
    Debug.scriptSource(ChooseAnimal).replace('Cat', "Cap' + 'y' + 'bara");
print('new source: ' + new_source);

%LiveEditPatchScript(ChooseAnimal, new_source);

assertEquals("Capybara", foo());
