//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function foo() {
    // override global accessor property 'foo' with a function
    // this should convert the property to a data property with
    // writable true, enumerable true, configurable false
}
eval("function bar() { /* same deal except for eval defined global functions configurable will be true */ }");

(function verifyGlobalPropertyDescriptors() {
    var d = Object.getOwnPropertyDescriptor(this, 'foo');

    assertPropertyDoesNotExist(d, 'get');
    assertPropertyDoesNotExist(d, 'set');
    assertPropertyExists(d, 'configurable', false);
    assertPropertyExists(d, 'enumerable', true);
    assertPropertyExists(d, 'writable', true);
    assertPropertyExists(d, 'value', foo);

    d = Object.getOwnPropertyDescriptor(this, 'bar');

    assertPropertyDoesNotExist(d, 'get');
    assertPropertyDoesNotExist(d, 'set');
    assertPropertyExists(d, 'configurable', true);
    assertPropertyExists(d, 'enumerable', true);
    assertPropertyExists(d, 'writable', true);
    assertPropertyExists(d, 'value', bar);
}).call(this);

try {
    eval("function nonConfigurableBar() { /* try to override non-configurable global accessor property with a function definition */ }");
} catch (e) {
    if (!(e instanceof TypeError) || e.message != "Cannot redefine non-configurable property 'nonConfigurableBar'")
        throw e;
}
