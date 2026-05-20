// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASE_IOS_HEADERS_H_
#define V8_BASE_IOS_HEADERS_H_

// This file includes the necessary headers that are not part of the
// iOS public SDK in order to support memory allocation on iOS.

#include <mach/mach.h>
#include <mach/vm_map.h>

__BEGIN_DECLS

kern_return_t mach_vm_remap(
    vm_map_t target_task, mach_vm_address_t* target_address,
    mach_vm_size_t size, mach_vm_offset_t mask, int flags, vm_map_t src_task,
    mach_vm_address_t src_address, boolean_t copy, vm_prot_t* cur_protection,
    vm_prot_t* max_protection, vm_inherit_t inheritance);

kern_return_t mach_vm_map(vm_map_t target_task, mach_vm_address_t* address,
                          mach_vm_size_t size, mach_vm_offset_t mask, int flags,
                          mem_entry_name_port_t object,
                          memory_object_offset_t offset, boolean_t copy,
                          vm_prot_t cur_protection, vm_prot_t max_protection,
                          vm_inherit_t inheritance);

__END_DECLS

#endif  // V8_BASE_IOS_HEADERS_H_
