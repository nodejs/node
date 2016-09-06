# Copyright 2015 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
   'targets': [
     {
       'target_name': 'asan_dynamic_runtime',
       'toolsets': ['target', 'host'],
       'type': 'none',
       'variables': {
         # Every target is going to depend on asan_dynamic_runtime, so allow
         # this one to depend on itself.
         'prune_self_dependency': 1,
         # Path is relative to this GYP file.
         'asan_rtl_mask_path':
             '../../third_party/llvm-build/Release+Asserts/lib/clang/*/lib/darwin',
         'asan_osx_dynamic':
             '<(asan_rtl_mask_path)/libclang_rt.asan_osx_dynamic.dylib',
       },
       'copies': [
         {
           'destination': '<(PRODUCT_DIR)',
           'files': [
             '<!(/bin/ls <(asan_osx_dynamic))',
           ],
         },
       ],
     },
   ],
}
