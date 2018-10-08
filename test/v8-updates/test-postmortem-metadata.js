'use strict';

// This test verifies that expected postmortem debugging metadata is present
// in the Node.js binary. These constants are used by tools such as llnode and
// mdb_v8, so this test ensures that they don't vanish between V8 updates.

const common = require('../common');
const assert = require('assert');
const { spawnSync } = require('child_process');
const { getSharedLibPath } = require('../common/shared-lib-util.js');

// For shared lib case, check shared lib instead
const args = [
  process.config.variables.node_shared ?
    getSharedLibPath() : process.execPath
];

if (common.isAIX)
  args.unshift('-Xany', '-B');

if (common.isOpenBSD)
  common.skip('no v8 debug symbols on OpenBSD');

const nm = spawnSync('nm', args);

if (nm.error && nm.error.errno === 'ENOENT')
  common.skip('nm not found on system');

const stderr = nm.stderr.toString();
if (stderr.length > 0) {
  common.skip(`Failed to execute nm: ${stderr}`);
}

const symbolRe = /\s_?(v8dbg_.+)$/;
const symbols = nm.stdout.toString().split('\n').reduce((filtered, line) => {
  const match = line.match(symbolRe);
  const symbol = match && match[1];

  if (symbol)
    filtered.push(symbol);

  return filtered;
}, []);
const missing = getExpectedSymbols().filter((symbol) => {
  return !symbols.includes(symbol);
});

assert.strictEqual(missing.length, 0, `Missing constants: ${missing}`);

// This is only a function so that the long list of expected symbols can be
// pushed to the bottom of the file for improved readability.
function getExpectedSymbols() {
  return [
    // Disable the maximum line length for the remainder of the file since it
    // should only consist of postmortem constants, and some of them can be
    // relatively long.
    /* eslint-disable max-len */
    'v8dbg_bit_field3_is_dictionary_map_shift',
    'v8dbg_bit_field3_number_of_own_descriptors_shift',
    'v8dbg_class_Code__instruction_size__int',
    'v8dbg_class_Code__instruction_start__uintptr_t',
    'v8dbg_class_ConsString__first__String',
    'v8dbg_class_ConsString__second__String',
    'v8dbg_class_FixedArray__data__uintptr_t',
    'v8dbg_class_FixedArrayBase__length__SMI',
    'v8dbg_class_FixedTypedArrayBase__base_pointer__Object',
    'v8dbg_class_FixedTypedArrayBase__external_pointer__Object',
    'v8dbg_class_HeapNumber__value__double',
    'v8dbg_class_HeapObject__map__Map',
    'v8dbg_class_JSArray__length__Object',
    'v8dbg_class_JSArrayBuffer__backing_store__Object',
    'v8dbg_class_JSArrayBuffer__byte_length__size_t',
    'v8dbg_class_JSArrayBufferView__buffer__Object',
    'v8dbg_class_JSArrayBufferView__byte_length__size_t',
    'v8dbg_class_JSArrayBufferView__byte_offset__size_t',
    'v8dbg_class_JSDate__value__Object',
    'v8dbg_class_JSFunction__context__Context',
    'v8dbg_class_JSFunction__shared__SharedFunctionInfo',
    'v8dbg_class_JSObject__elements__Object',
    'v8dbg_class_JSObject__internal_fields__uintptr_t',
    'v8dbg_class_JSReceiver__raw_properties_or_hash__Object',
    'v8dbg_class_JSRegExp__source__Object',
    'v8dbg_class_Map__bit_field3__int',
    'v8dbg_class_Map__constructor_or_backpointer__Object',
    'v8dbg_class_Map__inobject_properties_start_or_constructor_function_index__char',
    'v8dbg_class_Map__instance_type__uint16_t',
    'v8dbg_class_Map__instance_descriptors__DescriptorArray',
    'v8dbg_class_Map__instance_size_in_words__char',
    'v8dbg_class_Oddball__kind_offset__int',
    'v8dbg_class_Script__line_ends__Object',
    'v8dbg_class_Script__line_offset__SMI',
    'v8dbg_class_Script__name__Object',
    'v8dbg_class_Script__source__Object',
    'v8dbg_class_SeqOneByteString__chars__char',
    'v8dbg_class_SeqTwoByteString__chars__char',
    'v8dbg_class_SharedFunctionInfo__function_data__Object',
    'v8dbg_class_SharedFunctionInfo__flags__int',
    'v8dbg_class_UncompiledData__end_position__int32_t',
    'v8dbg_class_UncompiledData__inferred_name__String',
    'v8dbg_class_SharedFunctionInfo__internal_formal_parameter_count__uint16_t',
    'v8dbg_class_SharedFunctionInfo__name_or_scope_info__Object',
    'v8dbg_class_SharedFunctionInfo__script_or_debug_info__Object',
    'v8dbg_class_UncompiledData__start_position__int32_t',
    'v8dbg_class_SlicedString__offset__SMI',
    'v8dbg_class_SlicedString__parent__String',
    'v8dbg_class_String__length__int32_t',
    'v8dbg_class_ThinString__actual__String',
    'v8dbg_context_idx_scope_info',
    'v8dbg_context_idx_prev',
    'v8dbg_context_min_slots',
    'v8dbg_frametype_ArgumentsAdaptorFrame',
    'v8dbg_frametype_ConstructEntryFrame',
    'v8dbg_frametype_ConstructFrame',
    'v8dbg_frametype_EntryFrame',
    'v8dbg_frametype_ExitFrame',
    'v8dbg_frametype_InternalFrame',
    'v8dbg_frametype_OptimizedFrame',
    'v8dbg_frametype_StubFrame',
    'v8dbg_jsarray_buffer_was_neutered_mask',
    'v8dbg_jsarray_buffer_was_neutered_shift',
    'v8dbg_namedictionary_prefix_start_index',
    'v8dbg_namedictionaryshape_entry_size',
    'v8dbg_namedictionaryshape_prefix_size',
    'v8dbg_off_fp_args',
    'v8dbg_off_fp_context',
    'v8dbg_off_fp_function',
    'v8dbg_prop_attributes_mask',
    'v8dbg_prop_attributes_shift',
    'v8dbg_prop_attributes_DONT_ENUM',
    'v8dbg_prop_attributes_DONT_ENUM',
    'v8dbg_prop_attributes_NONE',
    'v8dbg_prop_attributes_READ_ONLY',
    'v8dbg_prop_desc_details',
    'v8dbg_prop_desc_key',
    'v8dbg_prop_desc_size',
    'v8dbg_prop_desc_value',
    'v8dbg_prop_idx_first',
    'v8dbg_prop_index_mask',
    'v8dbg_prop_index_shift',
    'v8dbg_prop_kind_mask',
    'v8dbg_prop_kind_Accessor',
    'v8dbg_prop_kind_Data',
    'v8dbg_prop_location_mask',
    'v8dbg_prop_location_shift',
    'v8dbg_prop_location_Descriptor',
    'v8dbg_prop_location_Field',
    'v8dbg_prop_representation_double',
    'v8dbg_prop_representation_mask',
    'v8dbg_prop_representation_shift',
    'v8dbg_scopeinfo_idx_first_vars',
    'v8dbg_scopeinfo_idx_ncontextlocals',
    'v8dbg_scopeinfo_idx_nparams',
    'v8dbg_type_Code__CODE_TYPE',
    'v8dbg_type_FixedArray__FIXED_ARRAY_TYPE',
    'v8dbg_type_HeapNumber__HEAP_NUMBER_TYPE',
    'v8dbg_type_JSArray__JS_ARRAY_TYPE',
    'v8dbg_type_JSArrayBuffer__JS_ARRAY_BUFFER_TYPE',
    'v8dbg_type_JSDate__JS_DATE_TYPE',
    'v8dbg_type_JSFunction__JS_FUNCTION_TYPE',
    'v8dbg_type_JSGlobalObject__JS_GLOBAL_OBJECT_TYPE',
    'v8dbg_type_JSGlobalProxy__JS_GLOBAL_PROXY_TYPE',
    'v8dbg_type_JSObject__JS_OBJECT_TYPE',
    'v8dbg_type_JSRegExp__JS_REGEXP_TYPE',
    'v8dbg_type_JSTypedArray__JS_TYPED_ARRAY_TYPE',
    'v8dbg_type_Map__MAP_TYPE',
    'v8dbg_type_Oddball__ODDBALL_TYPE',
    'v8dbg_type_Script__SCRIPT_TYPE',
    'v8dbg_type_SharedFunctionInfo__SHARED_FUNCTION_INFO_TYPE',
    'v8dbg_APIObjectType',
    'v8dbg_ConsStringTag',
    'v8dbg_ExternalStringTag',
    'v8dbg_FirstNonstringType',
    'v8dbg_HeapObjectTag',
    'v8dbg_HeapObjectTagMask',
    'v8dbg_OddballException',
    'v8dbg_OddballFalse',
    'v8dbg_OddballNull',
    'v8dbg_OddballTheHole',
    'v8dbg_OddballTrue',
    'v8dbg_OddballUndefined',
    'v8dbg_OddballUninitialized',
    'v8dbg_OneByteStringTag',
    'v8dbg_PointerSizeLog2',
    'v8dbg_SeqStringTag',
    'v8dbg_SlicedStringTag',
    'v8dbg_SmiShiftSize',
    'v8dbg_SmiTag',
    'v8dbg_SmiTagMask',
    'v8dbg_SpecialAPIObjectType',
    'v8dbg_StringEncodingMask',
    'v8dbg_StringRepresentationMask',
    'v8dbg_ThinStringTag',
    'v8dbg_TwoByteStringTag',
  ];
}
