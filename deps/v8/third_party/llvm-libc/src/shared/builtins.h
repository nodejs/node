//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This header aggregates LLVM-libc's shared compiler-rt builtins so that
/// they can be reused by compiler-rt's builtins.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SHARED_BUILTINS_H
#define LLVM_LIBC_SHARED_BUILTINS_H

#include "libc_common.h"

#include "builtins/adddf3.h"
#include "builtins/addsf3.h"
#include "builtins/addtf3.h"
#include "builtins/divdf3.h"
#include "builtins/divsf3.h"
#include "builtins/divtf3.h"
#include "builtins/extenddftf2.h"
#include "builtins/extendsfdf2.h"
#include "builtins/extendsftf2.h"
#include "builtins/extendxftf2.h"
#include "builtins/fixdfdi.h"
#include "builtins/fixdfsi.h"
#include "builtins/fixdfti.h"
#include "builtins/fixsfdi.h"
#include "builtins/fixsfsi.h"
#include "builtins/fixsfti.h"
#include "builtins/fixunsdfdi.h"
#include "builtins/fixunsdfsi.h"
#include "builtins/fixunsdfti.h"
#include "builtins/fixunssfdi.h"
#include "builtins/fixunssfsi.h"
#include "builtins/fixunssfti.h"
#include "builtins/floatdidf.h"
#include "builtins/floatdisf.h"
#include "builtins/floatsidf.h"
#include "builtins/floatsisf.h"
#include "builtins/floattidf.h"
#include "builtins/floattisf.h"
#include "builtins/floatundidf.h"
#include "builtins/floatundisf.h"
#include "builtins/floatunsidf.h"
#include "builtins/floatunsisf.h"
#include "builtins/floatuntidf.h"
#include "builtins/floatuntisf.h"
#include "builtins/muldf3.h"
#include "builtins/mulsf3.h"
#include "builtins/multf3.h"
#include "builtins/negdf2.h"
#include "builtins/negsf2.h"
#include "builtins/subdf3.h"
#include "builtins/subsf3.h"
#include "builtins/subtf3.h"
#include "builtins/truncdfsf2.h"
#include "builtins/trunctfdf2.h"
#include "builtins/trunctfsf2.h"
#include "builtins/trunctfxf2.h"

#endif // LLVM_LIBC_SHARED_BUILTINS_H
