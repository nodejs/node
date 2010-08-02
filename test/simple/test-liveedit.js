common = require("../common");
assert = require("assert");


// This is a duplicate of deps/v8/test/mjsunit/debug-liveedit-1.js
//  Just exercises the process.debug object.

eval("var something1 = 25; "
     + " function ChooseAnimal() { return          'Cat';          } "
     + " ChooseAnimal.Helper = function() { return 'Help!'; }");

assert.equal("Cat", ChooseAnimal());

var script = process.debug.findScript(ChooseAnimal);

var orig_animal = "Cat";
var patch_pos = script.source.indexOf(orig_animal);
var new_animal_patch = "Cap' + 'y' + 'bara";

var change_log = new Array();
process.debug.LiveEdit.TestApi.ApplySingleChunkPatch(script,
                                                     patch_pos,
                                                     orig_animal.length,
                                                     new_animal_patch,
                                                     change_log);

assert.equal("Capybara", ChooseAnimal());
