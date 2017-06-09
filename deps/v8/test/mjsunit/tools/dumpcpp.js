// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Load implementations from <project root>/tools.
// Files: tools/splaytree.js tools/codemap.js tools/csvparser.js
// Files: tools/consarray.js tools/profile.js tools/profile_view.js
// Files: tools/logreader.js tools/tickprocessor.js
// Files: tools/dumpcpp.js
// Env: TEST_FILE_NAME

(function testProcessSharedLibrary() {
  var oldLoadSymbols = UnixCppEntriesProvider.prototype.loadSymbols;

  UnixCppEntriesProvider.prototype.loadSymbols = function(libName) {
    this.symbols = [[
      '00000100 00000001 t v8::internal::Runtime_StringReplaceRegExpWithString(v8::internal::Arguments)',
      '00000110 00000001 T v8::internal::Runtime::GetElementOrCharAt(v8::internal::Handle<v8::internal::Object>, unsigned int)',
      '00000120 00000001 t v8::internal::Runtime_DebugGetPropertyDetails(v8::internal::Arguments)',
      '00000130 00000001 W v8::internal::RegExpMacroAssembler::CheckPosition(int, v8::internal::Label*)'
    ].join('\n'), ''];
  };

  var testCppProcessor = new CppProcessor(new UnixCppEntriesProvider(),
                                          false, false);
  testCppProcessor.processSharedLibrary(
    '/usr/local/google/home/lpy/v8/out/native/d8',
    0x00000100, 0x00000400, 0);

  var staticEntries = testCppProcessor.codeMap_.getAllStaticEntriesWithAddresses();
  var total = staticEntries.length;
  assertEquals(total, 3);
  assertEquals(staticEntries[0],
               [288,{size:1,
                     name:'v8::internal::Runtime_DebugGetPropertyDetails(v8::internal::Arguments)',
                     type:'CPP',
                     nameUpdated_:false}
               ]);
  assertEquals(staticEntries[1],
               [272,{size:1,
                     name:'v8::internal::Runtime::GetElementOrCharAt(v8::internal::Handle<v8::internal::Object>, unsigned int)',
                     type:'CPP',
                     nameUpdated_:false}
               ]);
  assertEquals(staticEntries[2],
              [256,{size:1,
                    name:'v8::internal::Runtime_StringReplaceRegExpWithString(v8::internal::Arguments)',
                    type:'CPP',
                    nameUpdated_:false}
              ]);

  UnixCppEntriesProvider.prototype.loadSymbols = oldLoadSymbols;
})();
