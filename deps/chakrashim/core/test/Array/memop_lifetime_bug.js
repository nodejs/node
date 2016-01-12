//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function lifetime_memset(arr, n) {
  if(arr[n - 1] !== 0) {
    for(var i = 0; i < n; ++i) {
      arr[i] = 0;
    }
  }
}

function lifetime_memcopy(arr, arr2, n) {
  if(arr[n - 1] !== 1) {
    for(var i = 0; i < n; ++i) {
      arr[i] = arr2[i];
    }
  }
}
var n = 8;
var arr = Array(n);
var arr2 = Array(n);
lifetime_memset(arr, n);
lifetime_memset(arr2, n);

lifetime_memcopy(arr, arr2, n);
lifetime_memcopy(arr, arr2, n);
print("PASSED");
