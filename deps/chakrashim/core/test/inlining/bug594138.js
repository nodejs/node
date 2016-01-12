//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// TH: 594138

while (true) {
  var v13 = {
      v14: function () {
        return function bar() {
          this.method0.apply(this.prop2, arguments);
        };
      }
    };
  var v15 = {};
  v15.v16 = v13.v14();
  v15.v16.prototype = {
    method0: function () {
      this.v20;
    }
  };
  v15.v18 = v13.v14();
  v15.v18.prototype = {
    method0: function () {
      new v15.v16();
    }
  };
  var v35 = new v15.v18();
  var v36 = new v15.v18();
  break;
}

WScript.Echo("PASSED");