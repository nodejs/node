//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

//Bug : 480890
//Flags:  -bgjit- -loopinterpretcount:1 -off:aggressiveinttypespec  -maxlinearintcasecount:1
  var obj0 = {}; 
  var g = 1;

  var __loopvar0 = 0;
  do {
    __loopvar0++;
    
    switch(g) {
      case 1: 
        break;
      case (a() ? 1 : (g)): 
        break;
      case 4:  
      case -3: 
      case 2:       
    }
    g == 1;
  } while(__loopvar0 < 2)
  
  WScript.Echo("PASSED");
