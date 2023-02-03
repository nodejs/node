// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TURBOSHAFT_DECOMPRESSION_OPTIMIZATION_H_
#define V8_COMPILER_TURBOSHAFT_DECOMPRESSION_OPTIMIZATION_H_

namespace v8::internal {
class Zone;
}
namespace v8::internal::compiler::turboshaft {
class Graph;

// The purpose of decompression optimization is to avoid unnecessary pointer
// decompression operations. If a compressed value loaded from the heap is only
// used as a Smi or to store it back into the heap, then there is no need to add
// the root pointer to make it dereferencable. By performing this optimization
// late in the pipeline, all the preceding phases can safely assume that
// everything is decompressed and do not need to worry about the distinction
// between compressed and uncompressed pointers.
void RunDecompressionOptimization(Graph& graph, Zone* phase_zone);

}  // namespace v8::internal::compiler::turboshaft

#endif  // V8_COMPILER_TURBOSHAFT_DECOMPRESSION_OPTIMIZATION_H_
