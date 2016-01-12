//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

(function () {
  with ({}) {
    try {
      arguments(z) = w; // shouldn't crash
    } catch (e) {
    }
  }
})();

WScript.Echo("passed");
