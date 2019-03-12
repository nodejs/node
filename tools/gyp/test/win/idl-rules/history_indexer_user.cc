// Copyright (c) 2012 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "history_indexer.h"

// Use the thing in the IDL.
int main() {
  IChromeHistoryIndexer** indexer = 0;
  IID fake_iid;
  CoCreateInstance(fake_iid, NULL, CLSCTX_INPROC,
                   __uuidof(IChromeHistoryIndexer),
                   reinterpret_cast<void**>(indexer));
  return 0;
}
