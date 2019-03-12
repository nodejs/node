// Copyright (c) 2015 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/systm.h>
#include <mach/mach_types.h>

kern_return_t GypKext_start(kmod_info_t* ki, void* d) {
  printf("GypKext has started.\n");
  return KERN_SUCCESS;
}

kern_return_t GypKext_stop(kmod_info_t* ki, void* d) {
  printf("GypKext has stopped.\n");
  return KERN_SUCCESS;
}
