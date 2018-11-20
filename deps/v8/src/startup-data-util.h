// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef V8_STARTUP_DATA_UTIL_H_
#define V8_STARTUP_DATA_UTIL_H_

#include "include/v8.h"

namespace v8 {

#ifdef V8_USE_EXTERNAL_STARTUP_DATA
// Helper class to load the startup data files from disk.
//
// This is meant as a convenience for stand-alone binaries like d8, cctest,
// unittest. A V8 embedder would likely either handle startup data on their
// own or just disable the feature if they don't want to handle it at all,
// while tools like cctest need to work in either configuration. Hence this is
// not meant for inclusion in the general v8 library.
class StartupDataHandler {
 public:
  // Load startup data, and call the v8::V8::Set*DataBlob API functions.
  //
  // natives_blob and snapshot_blob will be loaded realitive to exec_path,
  // which would usually be the equivalent of argv[0].
  StartupDataHandler(const char* exec_path, const char* natives_blob,
                     const char* snapshot_blob);
  ~StartupDataHandler();

 private:
  static char* RelativePath(char** buffer, const char* exec_path,
                            const char* name);

  void LoadFromFiles(const char* natives_blob, const char* snapshot_blob);

  void Load(const char* blob_file, v8::StartupData* startup_data,
            void (*setter_fn)(v8::StartupData*));

  v8::StartupData natives_;
  v8::StartupData snapshot_;

  // Disallow copy & assign.
  StartupDataHandler(const StartupDataHandler& other);
  void operator=(const StartupDataHandler& other);
};
#endif  // V8_USE_EXTERNAL_STARTUP_DATA

}  // namespace v8

#endif  // V8_STARTUP_DATA_UTIL_H_
