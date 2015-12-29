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

// Properties for heap statistics buffer extraction.
const heapStatisticsBuffer =
    new Uint32Array(v8binding.heapStatisticsArrayBuffer);
const kTotalHeapSizeIndex = v8binding.kTotalHeapSizeIndex;
const kTotalHeapSizeExecutableIndex = v8binding.kTotalHeapSizeExecutableIndex;
const kTotalPhysicalSizeIndex = v8binding.kTotalPhysicalSizeIndex;
const kTotalAvailableSize = v8binding.kTotalAvailableSize;
const kUsedHeapSizeIndex = v8binding.kUsedHeapSizeIndex;
const kHeapSizeLimitIndex = v8binding.kHeapSizeLimitIndex;

// Properties for heap space statistics buffer extraction.
const heapSpaceStatisticsBuffer =
    new Uint32Array(v8binding.heapSpaceStatisticsArrayBuffer);
const kHeapSpaces = v8binding.kHeapSpaces;
const kNumberOfHeapSpaces = kHeapSpaces.length;
const kHeapSpaceStatisticsPropertiesCount =
    v8binding.kHeapSpaceStatisticsPropertiesCount;
const kSpaceSizeIndex = v8binding.kSpaceSizeIndex;
const kSpaceUsedSizeIndex = v8binding.kSpaceUsedSizeIndex;
const kSpaceAvailableSizeIndex = v8binding.kSpaceAvailableSizeIndex;
const kPhysicalSpaceSizeIndex = v8binding.kPhysicalSpaceSizeIndex;

exports.getHeapStatistics = function() {
  const buffer = heapStatisticsBuffer;

  v8binding.updateHeapStatisticsArrayBuffer();

  return {
    'total_heap_size': buffer[kTotalHeapSizeIndex],
    'total_heap_size_executable': buffer[kTotalHeapSizeExecutableIndex],
    'total_physical_size': buffer[kTotalPhysicalSizeIndex],
    'total_available_size': buffer[kTotalAvailableSize],
    'used_heap_size': buffer[kUsedHeapSizeIndex],
    'heap_size_limit': buffer[kHeapSizeLimitIndex]
  };
};

exports.setFlagsFromString = v8binding.setFlagsFromString;

exports.getHeapSpaceStatistics = function() {
  const heapSpaceStatistics = new Array(kNumberOfHeapSpaces);
  const buffer = heapSpaceStatisticsBuffer;
  v8binding.updateHeapSpaceStatisticsArrayBuffer();

  for (let i = 0; i < kNumberOfHeapSpaces; i++) {
    const propertyOffset = i * kHeapSpaceStatisticsPropertiesCount;
    heapSpaceStatistics[i] = {
      space_name: kHeapSpaces[i],
      space_size: buffer[propertyOffset + kSpaceSizeIndex],
      space_used_size: buffer[propertyOffset + kSpaceUsedSizeIndex],
      space_available_size: buffer[propertyOffset + kSpaceAvailableSizeIndex],
      physical_space_size: buffer[propertyOffset + kPhysicalSpaceSizeIndex]
    };
  }

  return heapSpaceStatistics;
};
