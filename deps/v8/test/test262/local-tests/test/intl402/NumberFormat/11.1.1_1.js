// Copyright 2012 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 11.1.1_1
description: Tests that the this-value is ignored in NumberFormat.
author: Norbert Lindenberg
includes: [testIntl.js]
---*/

testWithIntlConstructors(function (Constructor) {
    var obj, newObj;

    if (Constructor === Intl.NumberFormat) {
      obj = new Constructor();
      newObj = Intl.NumberFormat.call(obj);
      if (obj !== newObj) {
        $ERROR("Should have modified existing object.");
      }
      var key = Object.getOwnPropertySymbols(newObj)[0];
      if (!(newObj[key] instanceof Intl.NumberFormat)) {
        $ERROR("Should have installed a NumberFormat instance.");
      }
      return true;
    }

    // variant 1: use constructor in a "new" expression
    obj = new Constructor();
    newObj = Intl.NumberFormat.call(obj);
    if (obj === newObj) {
      $ERROR("NumberFormat object created with \"new\" was not ignored as this-value.");
    }

    // variant 2: use constructor as a function
    obj = Constructor();
    newObj = Intl.NumberFormat.call(obj);
    if (obj === newObj) {
      $ERROR("NumberFormat object created with constructor as function was not ignored as this-value.");
    }

    return true;
});
