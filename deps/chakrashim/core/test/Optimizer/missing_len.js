//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

//FileName: \\ezeqasrv1\users\runtimetoolsbugs\casanalyzed\exprgen\fbl_ie_script_dev\130228-2030\x86\nebularun_x86\201303012321\ddltd2_0c68bf08-e27f-4a80-b0be-afca5b36c627\bug0.js
//Configuration: cse.xml
//Testcase Number: 8661
//Switches:  -maxinterpretcount:1  
//Branch:  fbl_ie_script_dev
//Build: 130228-2030
//Arch: X86
//MachineName: BPT30135
function test2(){
  var ary = new Array(10);
  var d = 1;
  ary[0] = 1;
  ary[1] = 1;
  ary[2] = 1;
  ary[3] = 1;
  ary[4] = 1;
  ary[5] = 1;
  // Snippets: implicitcalls2.ecs
  function v292101()
  {
    Object.defineProperty(Array.prototype, "4", {configurable : true, get: function(){ return 30;}});
    //Code Snippet: Seal.ecs 
    var _obj = new Object()
    _obj.x = ((d ? ary.length : ary.length) ? ary[(4)] : 1);
    WScript.Echo("_obj.x = " + _obj.x);
  }
  
  v292101();
  v292101();
};

// generate profile
test2();

// run JITted code
test2();


