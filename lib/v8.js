// Copyright (c) 2014, StrongLoop Inc.
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
// WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
// ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
// WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
// ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
// OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

'use strict';

const v8binding = process.binding('v8');
const smalloc = require('smalloc');

const heapStatisticsBuffer =
    smalloc.alloc(v8binding.kHeapStatisticsBufferLength,
                  v8binding.kHeapStatisticsBufferType);

const kTotalHeapSizeIndex = v8binding.kTotalHeapSizeIndex;
const kTotalHeapSizeExecutableIndex = v8binding.kTotalHeapSizeExecutableIndex;
const kTotalPhysicalSizeIndex = v8binding.kTotalPhysicalSizeIndex;
const kUsedHeapSizeIndex = v8binding.kUsedHeapSizeIndex;
const kHeapSizeLimitIndex = v8binding.kHeapSizeLimitIndex;

exports.getHeapStatistics = function() {
  var buffer = heapStatisticsBuffer;

  v8binding.getHeapStatistics(buffer);

  return {
    'total_heap_size': buffer[kTotalHeapSizeIndex],
    'total_heap_size_executable': buffer[kTotalHeapSizeExecutableIndex],
    'total_physical_size': buffer[kTotalPhysicalSizeIndex],
    'used_heap_size': buffer[kUsedHeapSizeIndex],
    'heap_size_limit': buffer[kHeapSizeLimitIndex]
  };
};

exports.setFlagsFromString = v8binding.setFlagsFromString;
