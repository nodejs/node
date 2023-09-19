// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const UNCLASSIFIED_CATEGORY = 'unclassified';
const UNCLASSIFIED_CATEGORY_NAME = 'Unclassified';

// Categories for zones.
export const CATEGORIES = new Map([
  [
    'parser', new Set([
      'AstStringConstants',
      'parser-zone',
      'pre-parser-zone',
    ])
  ],
  [
    'misc', new Set([
      'Run',
      'CanonicalHandleScope',
      'Temporary scoped zone',
      'UpdateFieldType',
    ])
  ],
  [
    'interpreter', new Set([
      'InterpreterCompilationJob',
    ])
  ],
  [
    'regexp', new Set([
      'CompileIrregexp',
    ])
  ],
  [
    'compiler-huge', new Set([
      'graph-zone',
      'instruction-zone',
      'pipeline-compilation-job-zone',
      'register-allocation-zone',
      'register-allocator-verifier-zone',
    ])
  ],
  [
    'compiler-other', new Set([
      'Compile',
      'V8.TFAllocateFPRegisters',
      'V8.TFAllocateGeneralRegisters',
      'V8.TFAssembleCode',
      'V8.TFAssignSpillSlots',
      'V8.TFBitcastElision',
      'V8.TFBuildLiveRangeBundles',
      'V8.TFBuildLiveRanges',
      'V8.TFBytecodeGraphBuilder',
      'V8.TFCommitAssignment',
      'V8.TFConnectRanges',
      'V8.TFControlFlowOptimization',
      'V8.TFDecideSpillingMode',
      'V8.TFDecompressionOptimization',
      'V8.TFEarlyOptimization',
      'V8.TFEarlyTrimming',
      'V8.TFEffectLinearization',
      'V8.TFEscapeAnalysis',
      'V8.TFFinalizeCode',
      'V8.TFFrameElision',
      'V8.TFGenericLowering',
      'V8.TFHeapBrokerInitialization',
      'V8.TFInlining',
      'V8.TFJumpThreading',
      'V8.TFLateGraphTrimming',
      'V8.TFLateOptimization',
      'V8.TFLoadElimination',
      'V8.TFLocateSpillSlots',
      'V8.TFLoopPeeling',
      'V8.TFMachineOperatorOptimization',
      'V8.TFMeetRegisterConstraints',
      'V8.TFMemoryOptimization',
      'V8.TFOptimizeMoves',
      'V8.TFPopulatePointerMaps',
      'V8.TFResolveControlFlow',
      'V8.TFResolvePhis',
      'V8.TFScheduling',
      'V8.TFSelectInstructions',
      'V8.TFSerializeMetadata',
      'V8.TFSimplifiedLowering',
      'V8.TFStoreStoreElimination',
      'V8.TFTypedLowering',
      'V8.TFTyper',
      'V8.TFUntyper',
      'V8.TFVerifyGraph',
      'ValidatePendingAssessment',
      'codegen-zone',
    ])
  ],
  [UNCLASSIFIED_CATEGORY, new Set()],
]);

// Maps category to description text that is shown in html.
export const CATEGORY_NAMES = new Map([
  ['parser', 'Parser'],
  ['misc', 'Misc'],
  ['interpreter', 'Ignition'],
  ['regexp', 'Regexp compiler'],
  ['compiler-huge', 'TurboFan (huge zones)'],
  ['compiler-other', 'TurboFan (other zones)'],
  [UNCLASSIFIED_CATEGORY, UNCLASSIFIED_CATEGORY_NAME],
]);

function buildZoneToCategoryMap() {
  const map = new Map();
  for (let [category, zone_names] of CATEGORIES.entries()) {
    for (let zone_name of zone_names) {
      if (map.has(zone_name)) {
        console.error("Zone belongs to multiple categories: " + zone_name);
      } else {
        map.set(zone_name, category);
      }
    }
  }
  return map;
}

const CATEGORY_BY_ZONE = buildZoneToCategoryMap();

// Maps zone name to category.
export function categoryByZoneName(zone_name) {
  const category = CATEGORY_BY_ZONE.get(zone_name);
  if (category !== undefined) return category;
  return UNCLASSIFIED_CATEGORY;
}
