// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function opt() {
  try{
    Object.seal({})
  }finally{
    try{
      // Carefully crafted by clusterfuzz to alias the temporary object literal
      // register with the below dead try block's context register.
      (
        {
          toString(){
          }
        }
      ).apply(-1).x(  )
    }
    finally{
      if(2.2)
      {
        return
      }
      // This code should be dead.
      try{
        Reflect.construct
      }finally{
      }
    }
  }
}

opt();
%OptimizeFunctionOnNextCall(opt);
opt();
