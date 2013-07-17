/* Copyright Joyent, Inc. and other Node contributors. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

/*
 * mdb_v8_cfg.c: canned configurations for previous V8 versions.
 *
 * The functions and data defined here enable this dmod to support debugging
 * Node.js binaries that predated V8's built-in postmortem debugging support.
 */

#include "v8cfg.h"

/*ARGSUSED*/
static int
v8cfg_target_iter(v8_cfg_t *cfgp, int (*func)(mdb_symbol_t *, void *),
    void *arg)
{
	return (mdb_symbol_iter(MDB_OBJ_EVERY, MDB_DYNSYM,
	    MDB_BIND_GLOBAL | MDB_TYPE_OBJECT | MDB_TYPE_FUNC,
	    func, arg));
}

/*ARGSUSED*/
static int
v8cfg_target_readsym(v8_cfg_t *cfgp, const char *name, intptr_t *valp)
{
	int val, rval;

	if ((rval = mdb_readsym(&val, sizeof (val), name)) != -1)
		*valp = (intptr_t)val;

	return (rval);
}

/*
 * Analog of mdb_symbol_iter() for a canned configuration.
 */
static int
v8cfg_canned_iter(v8_cfg_t *cfgp, int (*func)(mdb_symbol_t *, void *),
    void *arg)
{
	v8_cfg_symbol_t *v8sym;
	mdb_symbol_t mdbsym;
	int rv;

	for (v8sym = cfgp->v8cfg_symbols; v8sym->v8cs_name != NULL; v8sym++) {
		mdbsym.sym_name = v8sym->v8cs_name;
		mdbsym.sym_object = NULL;
		mdbsym.sym_sym = NULL;
		mdbsym.sym_table = 0;
		mdbsym.sym_id = 0;

		if ((rv = func(&mdbsym, arg)) != 0)
			return (rv);
	}

	return (0);
}

/*
 * Analog of mdb_readsym() for a canned configuration.
 */
static int
v8cfg_canned_readsym(v8_cfg_t *cfgp, const char *name, intptr_t *valp)
{
	v8_cfg_symbol_t *v8sym;

	for (v8sym = cfgp->v8cfg_symbols; v8sym->v8cs_name != NULL; v8sym++) {
		if (strcmp(name, v8sym->v8cs_name) == 0)
			break;
	}

	if (v8sym->v8cs_name == NULL)
		return (-1);

	*valp = v8sym->v8cs_value;
	return (0);
}

/*
 * Canned configuration for the V8 bundled with Node.js v0.4.8 and later.
 */
static v8_cfg_symbol_t v8_symbols_node_04[] = {
	{ "v8dbg_type_AccessCheckInfo__ACCESS_CHECK_INFO_TYPE", 0x91 },
	{ "v8dbg_type_AccessorInfo__ACCESSOR_INFO_TYPE", 0x90 },
	{ "v8dbg_type_BreakPointInfo__BREAK_POINT_INFO_TYPE", 0x9b },
	{ "v8dbg_type_ByteArray__BYTE_ARRAY_TYPE", 0x86 },
	{ "v8dbg_type_CallHandlerInfo__CALL_HANDLER_INFO_TYPE", 0x93 },
	{ "v8dbg_type_Code__CODE_TYPE", 0x81 },
	{ "v8dbg_type_CodeCache__CODE_CACHE_TYPE", 0x99 },
	{ "v8dbg_type_ConsString__CONS_ASCII_STRING_TYPE", 0x5 },
	{ "v8dbg_type_ConsString__CONS_ASCII_SYMBOL_TYPE", 0x45 },
	{ "v8dbg_type_ConsString__CONS_STRING_TYPE", 0x1 },
	{ "v8dbg_type_ConsString__CONS_SYMBOL_TYPE", 0x41 },
	{ "v8dbg_type_DebugInfo__DEBUG_INFO_TYPE", 0x9a },
	{ "v8dbg_type_ExternalAsciiString__EXTERNAL_ASCII_STRING_TYPE", 0x6 },
	{ "v8dbg_type_ExternalAsciiString__EXTERNAL_ASCII_SYMBOL_TYPE", 0x46 },
	{ "v8dbg_type_ExternalByteArray__EXTERNAL_BYTE_ARRAY_TYPE", 0x88 },
	{ "v8dbg_type_ExternalFloatArray__EXTERNAL_FLOAT_ARRAY_TYPE", 0x8e },
	{ "v8dbg_type_ExternalIntArray__EXTERNAL_INT_ARRAY_TYPE", 0x8c },
	{ "v8dbg_type_ExternalShortArray__EXTERNAL_SHORT_ARRAY_TYPE", 0x8a },
	{ "v8dbg_type_ExternalString__EXTERNAL_STRING_TYPE", 0x2 },
	{ "v8dbg_type_ExternalString__EXTERNAL_SYMBOL_TYPE", 0x42 },
	{ "v8dbg_type_ExternalUnsignedByteArray__"
		"EXTERNAL_UNSIGNED_BYTE_ARRAY_TYPE", 0x89 },
	{ "v8dbg_type_ExternalUnsignedIntArray__"
		"EXTERNAL_UNSIGNED_INT_ARRAY_TYPE", 0x8d },
	{ "v8dbg_type_ExternalUnsignedShortArray__"
		"EXTERNAL_UNSIGNED_SHORT_ARRAY_TYPE", 0x8b },
	{ "v8dbg_type_FixedArray__FIXED_ARRAY_TYPE", 0x9c },
	{ "v8dbg_type_FunctionTemplateInfo__"
		"FUNCTION_TEMPLATE_INFO_TYPE", 0x94 },
	{ "v8dbg_type_HeapNumber__HEAP_NUMBER_TYPE", 0x84 },
	{ "v8dbg_type_InterceptorInfo__INTERCEPTOR_INFO_TYPE", 0x92 },
	{ "v8dbg_type_JSArray__JS_ARRAY_TYPE", 0xa5 },
	{ "v8dbg_type_JSBuiltinsObject__JS_BUILTINS_OBJECT_TYPE", 0xa3 },
	{ "v8dbg_type_JSFunction__JS_FUNCTION_TYPE", 0xa7 },
	{ "v8dbg_type_JSGlobalObject__JS_GLOBAL_OBJECT_TYPE", 0xa2 },
	{ "v8dbg_type_JSGlobalPropertyCell__"
		"JS_GLOBAL_PROPERTY_CELL_TYPE", 0x83 },
	{ "v8dbg_type_JSGlobalProxy__JS_GLOBAL_PROXY_TYPE", 0xa4 },
	{ "v8dbg_type_JSMessageObject__JS_MESSAGE_OBJECT_TYPE", 0x9e },
	{ "v8dbg_type_JSObject__JS_OBJECT_TYPE", 0xa0 },
	{ "v8dbg_type_JSRegExp__JS_REGEXP_TYPE", 0xa6 },
	{ "v8dbg_type_JSValue__JS_VALUE_TYPE", 0x9f },
	{ "v8dbg_type_Map__MAP_TYPE", 0x80 },
	{ "v8dbg_type_ObjectTemplateInfo__OBJECT_TEMPLATE_INFO_TYPE", 0x95 },
	{ "v8dbg_type_Oddball__ODDBALL_TYPE", 0x82 },
	{ "v8dbg_type_Script__SCRIPT_TYPE", 0x98 },
	{ "v8dbg_type_SeqAsciiString__ASCII_STRING_TYPE", 0x4 },
	{ "v8dbg_type_SeqAsciiString__ASCII_SYMBOL_TYPE", 0x44 },
	{ "v8dbg_type_SharedFunctionInfo__SHARED_FUNCTION_INFO_TYPE", 0x9d },
	{ "v8dbg_type_SignatureInfo__SIGNATURE_INFO_TYPE", 0x96 },
	{ "v8dbg_type_String__STRING_TYPE", 0x0 },
	{ "v8dbg_type_String__SYMBOL_TYPE", 0x40 },
	{ "v8dbg_type_TypeSwitchInfo__TYPE_SWITCH_INFO_TYPE", 0x97 },

	{ "v8dbg_class_AccessCheckInfo__data__Object", 0xc },
	{ "v8dbg_class_AccessCheckInfo__indexed_callback__Object", 0x8 },
	{ "v8dbg_class_AccessCheckInfo__named_callback__Object", 0x4 },
	{ "v8dbg_class_AccessorInfo__data__Object", 0xc },
	{ "v8dbg_class_AccessorInfo__flag__Smi", 0x14 },
	{ "v8dbg_class_AccessorInfo__getter__Object", 0x4 },
	{ "v8dbg_class_AccessorInfo__name__Object", 0x10 },
	{ "v8dbg_class_AccessorInfo__setter__Object", 0x8 },
	{ "v8dbg_class_BreakPointInfo__break_point_objects__Object", 0x10 },
	{ "v8dbg_class_BreakPointInfo__code_position__Smi", 0x4 },
	{ "v8dbg_class_BreakPointInfo__source_position__Smi", 0x8 },
	{ "v8dbg_class_BreakPointInfo__statement_position__Smi", 0xc },
	{ "v8dbg_class_ByteArray__length__SMI", 0x4 },
	{ "v8dbg_class_CallHandlerInfo__callback__Object", 0x4 },
	{ "v8dbg_class_CallHandlerInfo__data__Object", 0x8 },
	{ "v8dbg_class_Code__deoptimization_data__FixedArray", 0xc },
	{ "v8dbg_class_Code__instruction_size__int", 0x4 },
	{ "v8dbg_class_Code__instruction_start__int", 0x20 },
	{ "v8dbg_class_Code__relocation_info__ByteArray", 0x8 },
	{ "v8dbg_class_CodeCache__default_cache__FixedArray", 0x4 },
	{ "v8dbg_class_CodeCache__normal_type_cache__Object", 0x8 },
	{ "v8dbg_class_ConsString__first__String", 0xc },
	{ "v8dbg_class_ConsString__second__String", 0x10 },
	{ "v8dbg_class_DebugInfo__break_points__FixedArray", 0x14 },
	{ "v8dbg_class_DebugInfo__code__Code", 0xc },
	{ "v8dbg_class_DebugInfo__original_code__Code", 0x8 },
	{ "v8dbg_class_DebugInfo__shared__SharedFunctionInfo", 0x4 },
	{ "v8dbg_class_ExternalString__resource__Object", 0xc },
	{ "v8dbg_class_FixedArray__data__uintptr_t", 0x8 },
	{ "v8dbg_class_FixedArray__length__SMI", 0x4 },
	{ "v8dbg_class_FunctionTemplateInfo__access_check_info__Object", 0x38 },
	{ "v8dbg_class_FunctionTemplateInfo__call_code__Object", 0x10 },
	{ "v8dbg_class_FunctionTemplateInfo__class_name__Object", 0x2c },
	{ "v8dbg_class_FunctionTemplateInfo__flag__Smi", 0x3c },
	{ "v8dbg_class_FunctionTemplateInfo__"
		"indexed_property_handler__Object", 0x24 },
	{ "v8dbg_class_FunctionTemplateInfo__"
		"instance_call_handler__Object", 0x34 },
	{ "v8dbg_class_FunctionTemplateInfo__instance_template__Object", 0x28 },
	{ "v8dbg_class_FunctionTemplateInfo__"
		"named_property_handler__Object", 0x20 },
	{ "v8dbg_class_FunctionTemplateInfo__parent_template__Object", 0x1c },
	{ "v8dbg_class_FunctionTemplateInfo__"
		"property_accessors__Object", 0x14 },
	{ "v8dbg_class_FunctionTemplateInfo__"
		"prototype_template__Object", 0x18 },
	{ "v8dbg_class_FunctionTemplateInfo__serial_number__Object", 0xc },
	{ "v8dbg_class_FunctionTemplateInfo__signature__Object", 0x30 },
	{ "v8dbg_class_GlobalObject__builtins__JSBuiltinsObject", 0xc },
	{ "v8dbg_class_GlobalObject__global_context__Context", 0x10 },
	{ "v8dbg_class_GlobalObject__global_receiver__JSObject", 0x14 },
	{ "v8dbg_class_HeapNumber__value__SMI", 0x4 },
	{ "v8dbg_class_HeapObject__map__Map", 0x0 },
	{ "v8dbg_class_InterceptorInfo__data__Object", 0x18 },
	{ "v8dbg_class_InterceptorInfo__deleter__Object", 0x10 },
	{ "v8dbg_class_InterceptorInfo__enumerator__Object", 0x14 },
	{ "v8dbg_class_InterceptorInfo__getter__Object", 0x4 },
	{ "v8dbg_class_InterceptorInfo__query__Object", 0xc },
	{ "v8dbg_class_InterceptorInfo__setter__Object", 0x8 },
	{ "v8dbg_class_JSArray__length__Object", 0xc },
	{ "v8dbg_class_JSFunction__literals__FixedArray", 0x1c },
	{ "v8dbg_class_JSFunction__next_function_link__Object", 0x20 },
	{ "v8dbg_class_JSFunction__prototype_or_initial_map__Object", 0x10 },
	{ "v8dbg_class_JSFunction__shared__SharedFunctionInfo", 0x14 },
	{ "v8dbg_class_JSGlobalProxy__context__Object", 0xc },
	{ "v8dbg_class_JSMessageObject__arguments__JSArray", 0x10 },
	{ "v8dbg_class_JSMessageObject__end_position__SMI", 0x24 },
	{ "v8dbg_class_JSMessageObject__script__Object", 0x14 },
	{ "v8dbg_class_JSMessageObject__stack_frames__Object", 0x1c },
	{ "v8dbg_class_JSMessageObject__stack_trace__Object", 0x18 },
	{ "v8dbg_class_JSMessageObject__start_position__SMI", 0x20 },
	{ "v8dbg_class_JSMessageObject__type__String", 0xc },
	{ "v8dbg_class_JSObject__elements__Object", 0x8 },
	{ "v8dbg_class_JSObject__properties__FixedArray", 0x4 },
	{ "v8dbg_class_JSRegExp__data__Object", 0xc },
	{ "v8dbg_class_JSValue__value__Object", 0xc },
	{ "v8dbg_class_Map__code_cache__Object", 0x18 },
	{ "v8dbg_class_Map__constructor__Object", 0x10 },
	{ "v8dbg_class_Map__inobject_properties__int", 0x5 },
	{ "v8dbg_class_Map__instance_size__int", 0x4 },
	{ "v8dbg_class_Map__instance_attributes__int", 0x8 },
	{ "v8dbg_class_Map__instance_descriptors__DescriptorArray", 0x14 },
	{ "v8dbg_class_ObjectTemplateInfo__constructor__Object", 0xc },
	{ "v8dbg_class_ObjectTemplateInfo__"
		"internal_field_count__Object", 0x10 },
	{ "v8dbg_class_Oddball__to_number__Object", 0x8 },
	{ "v8dbg_class_Oddball__to_string__String", 0x4 },
	{ "v8dbg_class_Script__column_offset__Smi", 0x10 },
	{ "v8dbg_class_Script__compilation_type__Smi", 0x24 },
	{ "v8dbg_class_Script__context_data__Object", 0x18 },
	{ "v8dbg_class_Script__data__Object", 0x14 },
	{ "v8dbg_class_Script__eval_from_instructions_offset__Smi", 0x34 },
	{ "v8dbg_class_Script__eval_from_shared__Object", 0x30 },
	{ "v8dbg_class_Script__id__Object", 0x2c },
	{ "v8dbg_class_Script__line_ends__Object", 0x28 },
	{ "v8dbg_class_Script__line_offset__Smi", 0xc },
	{ "v8dbg_class_Script__name__Object", 0x8 },
	{ "v8dbg_class_Script__source__Object", 0x4 },
	{ "v8dbg_class_Script__type__Smi", 0x20 },
	{ "v8dbg_class_Script__wrapper__Proxy", 0x1c },
	{ "v8dbg_class_SeqAsciiString__chars__char", 0xc },
	{ "v8dbg_class_SharedFunctionInfo__code__Code", 0x8 },
	{ "v8dbg_class_SharedFunctionInfo__compiler_hints__SMI", 0x50 },
	{ "v8dbg_class_SharedFunctionInfo__construct_stub__Code", 0x10 },
	{ "v8dbg_class_SharedFunctionInfo__debug_info__Object", 0x20 },
	{ "v8dbg_class_SharedFunctionInfo__end_position__SMI", 0x48 },
	{ "v8dbg_class_SharedFunctionInfo__"
		"expected_nof_properties__SMI", 0x3c },
	{ "v8dbg_class_SharedFunctionInfo__formal_parameter_count__SMI", 0x38 },
	{ "v8dbg_class_SharedFunctionInfo__function_data__Object", 0x18 },
	{ "v8dbg_class_SharedFunctionInfo__"
		"function_token_position__SMI", 0x4c },
	{ "v8dbg_class_SharedFunctionInfo__inferred_name__String", 0x24 },
	{ "v8dbg_class_SharedFunctionInfo__initial_map__Object", 0x28 },
	{ "v8dbg_class_SharedFunctionInfo__instance_class_name__Object", 0x14 },
	{ "v8dbg_class_SharedFunctionInfo__length__SMI", 0x34 },
	{ "v8dbg_class_SharedFunctionInfo__name__Object", 0x4 },
	{ "v8dbg_class_SharedFunctionInfo__num_literals__SMI", 0x40 },
	{ "v8dbg_class_SharedFunctionInfo__opt_count__SMI", 0x58 },
	{ "v8dbg_class_SharedFunctionInfo__script__Object", 0x1c },
	{ "v8dbg_class_SharedFunctionInfo__"
		"start_position_and_type__SMI", 0x44 },
	{ "v8dbg_class_SharedFunctionInfo__"
		"this_property_assignments__Object", 0x2c },
	{ "v8dbg_class_SharedFunctionInfo__"
		"this_property_assignments_count__SMI", 0x54 },
	{ "v8dbg_class_SignatureInfo__args__Object", 0x8 },
	{ "v8dbg_class_SignatureInfo__receiver__Object", 0x4 },
	{ "v8dbg_class_String__length__SMI", 0x4 },
	{ "v8dbg_class_TemplateInfo__property_list__Object", 0x8 },
	{ "v8dbg_class_TemplateInfo__tag__Object", 0x4 },
	{ "v8dbg_class_TypeSwitchInfo__types__Object", 0x4 },

	{ "v8dbg_parent_AccessCheckInfo__Struct", 0x0 },
	{ "v8dbg_parent_AccessorInfo__Struct", 0x0 },
	{ "v8dbg_parent_BreakPointInfo__Struct", 0x0 },
	{ "v8dbg_parent_ByteArray__HeapObject", 0x0 },
	{ "v8dbg_parent_CallHandlerInfo__Struct", 0x0 },
	{ "v8dbg_parent_Code__HeapObject", 0x0 },
	{ "v8dbg_parent_CodeCache__Struct", 0x0 },
	{ "v8dbg_parent_ConsString__String", 0x0 },
	{ "v8dbg_parent_DebugInfo__Struct", 0x0 },
	{ "v8dbg_parent_DeoptimizationInputData__FixedArray", 0x0 },
	{ "v8dbg_parent_DeoptimizationOutputData__FixedArray", 0x0 },
	{ "v8dbg_parent_DescriptorArray__FixedArray", 0x0 },
	{ "v8dbg_parent_ExternalArray__HeapObject", 0x0 },
	{ "v8dbg_parent_ExternalAsciiString__ExternalString", 0x0 },
	{ "v8dbg_parent_ExternalByteArray__ExternalArray", 0x0 },
	{ "v8dbg_parent_ExternalFloatArray__ExternalArray", 0x0 },
	{ "v8dbg_parent_ExternalIntArray__ExternalArray", 0x0 },
	{ "v8dbg_parent_ExternalShortArray__ExternalArray", 0x0 },
	{ "v8dbg_parent_ExternalString__String", 0x0 },
	{ "v8dbg_parent_ExternalTwoByteString__ExternalString", 0x0 },
	{ "v8dbg_parent_ExternalUnsignedByteArray__ExternalArray", 0x0 },
	{ "v8dbg_parent_ExternalUnsignedIntArray__ExternalArray", 0x0 },
	{ "v8dbg_parent_ExternalUnsignedShortArray__ExternalArray", 0x0 },
	{ "v8dbg_parent_Failure__MaybeObject", 0x0 },
	{ "v8dbg_parent_FixedArray__HeapObject", 0x0 },
	{ "v8dbg_parent_FunctionTemplateInfo__TemplateInfo", 0x0 },
	{ "v8dbg_parent_GlobalObject__JSObject", 0x0 },
	{ "v8dbg_parent_HeapNumber__HeapObject", 0x0 },
	{ "v8dbg_parent_HeapObject__Object", 0x0 },
	{ "v8dbg_parent_InterceptorInfo__Struct", 0x0 },
	{ "v8dbg_parent_JSArray__JSObject", 0x0 },
	{ "v8dbg_parent_JSBuiltinsObject__GlobalObject", 0x0 },
	{ "v8dbg_parent_JSFunction__JSObject", 0x0 },
	{ "v8dbg_parent_JSFunctionResultCache__FixedArray", 0x0 },
	{ "v8dbg_parent_JSGlobalObject__GlobalObject", 0x0 },
	{ "v8dbg_parent_JSGlobalPropertyCell__HeapObject", 0x0 },
	{ "v8dbg_parent_JSGlobalProxy__JSObject", 0x0 },
	{ "v8dbg_parent_JSMessageObject__JSObject", 0x0 },
	{ "v8dbg_parent_JSObject__HeapObject", 0x0 },
	{ "v8dbg_parent_JSRegExp__JSObject", 0x0 },
	{ "v8dbg_parent_JSRegExpResult__JSArray", 0x0 },
	{ "v8dbg_parent_JSValue__JSObject", 0x0 },
	{ "v8dbg_parent_Map__HeapObject", 0x0 },
	{ "v8dbg_parent_NormalizedMapCache__FixedArray", 0x0 },
	{ "v8dbg_parent_Object__MaybeObject", 0x0 },
	{ "v8dbg_parent_ObjectTemplateInfo__TemplateInfo", 0x0 },
	{ "v8dbg_parent_Oddball__HeapObject", 0x0 },
	{ "v8dbg_parent_Script__Struct", 0x0 },
	{ "v8dbg_parent_SeqAsciiString__SeqString", 0x0 },
	{ "v8dbg_parent_SeqString__String", 0x0 },
	{ "v8dbg_parent_SeqTwoByteString__SeqString", 0x0 },
	{ "v8dbg_parent_SharedFunctionInfo__HeapObject", 0x0 },
	{ "v8dbg_parent_SignatureInfo__Struct", 0x0 },
	{ "v8dbg_parent_Smi__Object", 0x0 },
	{ "v8dbg_parent_String__HeapObject", 0x0 },
	{ "v8dbg_parent_Struct__HeapObject", 0x0 },
	{ "v8dbg_parent_TemplateInfo__Struct", 0x0 },
	{ "v8dbg_parent_TypeSwitchInfo__Struct", 0x0 },

	{ "v8dbg_frametype_ArgumentsAdaptorFrame", 0x8 },
	{ "v8dbg_frametype_ConstructFrame", 0x7 },
	{ "v8dbg_frametype_EntryConstructFrame", 0x2 },
	{ "v8dbg_frametype_EntryFrame", 0x1 },
	{ "v8dbg_frametype_ExitFrame", 0x3 },
	{ "v8dbg_frametype_InternalFrame", 0x6 },
	{ "v8dbg_frametype_JavaScriptFrame", 0x4 },
	{ "v8dbg_frametype_OptimizedFrame", 0x5 },

	{ "v8dbg_off_fp_context", -0x4 },
	{ "v8dbg_off_fp_function", -0x8 },
	{ "v8dbg_off_fp_marker", -0x8 },
	{ "v8dbg_off_fp_args", 0x8 },

	{ "v8dbg_prop_idx_content", 0x0 },
	{ "v8dbg_prop_idx_first", 0x2 },
	{ "v8dbg_prop_type_field", 0x1 },
	{ "v8dbg_prop_type_first_phantom", 0x6 },
	{ "v8dbg_prop_type_mask", 0xf },

	{ "v8dbg_AsciiStringTag", 0x4 },
	{ "v8dbg_ConsStringTag", 0x1 },
	{ "v8dbg_ExternalStringTag", 0x2 },
	{ "v8dbg_FailureTag", 0x3 },
	{ "v8dbg_FailureTagMask", 0x3 },
	{ "v8dbg_FirstNonstringType", 0x80 },
	{ "v8dbg_HeapObjectTag", 0x1 },
	{ "v8dbg_HeapObjectTagMask", 0x3 },
	{ "v8dbg_IsNotStringMask", 0x80 },
	{ "v8dbg_NotStringTag", 0x80 },
	{ "v8dbg_SeqStringTag", 0x0 },
	{ "v8dbg_SmiTag", 0x0 },
	{ "v8dbg_SmiTagMask", 0x1 },
	{ "v8dbg_SmiValueShift", 0x1 },
	{ "v8dbg_StringEncodingMask", 0x4 },
	{ "v8dbg_StringRepresentationMask", 0x3 },
	{ "v8dbg_StringTag", 0x0 },
	{ "v8dbg_TwoByteStringTag", 0x0 },
	{ "v8dbg_PointerSizeLog2", 0x2 },

	{ NULL }
};

/*
 * Canned configuration for the V8 bundled with Node.js v0.6.5.
 */
static v8_cfg_symbol_t v8_symbols_node_06[] = {
	{ "v8dbg_type_AccessCheckInfo__ACCESS_CHECK_INFO_TYPE", 0x93 },
	{ "v8dbg_type_AccessorInfo__ACCESSOR_INFO_TYPE", 0x92 },
	{ "v8dbg_type_BreakPointInfo__BREAK_POINT_INFO_TYPE", 0x9e },
	{ "v8dbg_type_ByteArray__BYTE_ARRAY_TYPE", 0x86 },
	{ "v8dbg_type_CallHandlerInfo__CALL_HANDLER_INFO_TYPE", 0x95 },
	{ "v8dbg_type_Code__CODE_TYPE", 0x81 },
	{ "v8dbg_type_CodeCache__CODE_CACHE_TYPE", 0x9b },
	{ "v8dbg_type_ConsString__CONS_ASCII_STRING_TYPE", 0x5 },
	{ "v8dbg_type_ConsString__CONS_ASCII_SYMBOL_TYPE", 0x45 },
	{ "v8dbg_type_ConsString__CONS_STRING_TYPE", 0x1 },
	{ "v8dbg_type_ConsString__CONS_SYMBOL_TYPE", 0x41 },
	{ "v8dbg_type_DebugInfo__DEBUG_INFO_TYPE", 0x9d },
	{ "v8dbg_type_ExternalAsciiString__EXTERNAL_ASCII_STRING_TYPE", 0x6 },
	{ "v8dbg_type_ExternalAsciiString__EXTERNAL_ASCII_SYMBOL_TYPE", 0x46 },
	{ "v8dbg_type_ExternalByteArray__EXTERNAL_BYTE_ARRAY_TYPE", 0x87 },
	{ "v8dbg_type_ExternalDoubleArray__EXTERNAL_DOUBLE_ARRAY_TYPE", 0x8e },
	{ "v8dbg_type_ExternalFloatArray__EXTERNAL_FLOAT_ARRAY_TYPE", 0x8d },
	{ "v8dbg_type_ExternalIntArray__EXTERNAL_INT_ARRAY_TYPE", 0x8b },
	{ "v8dbg_type_ExternalPixelArray__EXTERNAL_PIXEL_ARRAY_TYPE", 0x8f },
	{ "v8dbg_type_ExternalShortArray__EXTERNAL_SHORT_ARRAY_TYPE", 0x89 },
	{ "v8dbg_type_ExternalTwoByteString__EXTERNAL_STRING_TYPE", 0x2 },
	{ "v8dbg_type_ExternalTwoByteString__EXTERNAL_SYMBOL_TYPE", 0x42 },
	{ "v8dbg_type_ExternalUnsignedByteArray__"
		"EXTERNAL_UNSIGNED_BYTE_ARRAY_TYPE", 0x88 },
	{ "v8dbg_type_ExternalUnsignedIntArray__"
		"EXTERNAL_UNSIGNED_INT_ARRAY_TYPE", 0x8c },
	{ "v8dbg_type_ExternalUnsignedShortArray__"
		"EXTERNAL_UNSIGNED_SHORT_ARRAY_TYPE", 0x8a },
	{ "v8dbg_type_FixedArray__FIXED_ARRAY_TYPE", 0x9f },
	{ "v8dbg_type_FixedDoubleArray__FIXED_DOUBLE_ARRAY_TYPE", 0x90 },
	{ "v8dbg_type_Foreign__FOREIGN_TYPE", 0x85 },
	{ "v8dbg_type_FunctionTemplateInfo__FUNCTION_TEMPLATE_INFO_TYPE",
		0x96 },
	{ "v8dbg_type_HeapNumber__HEAP_NUMBER_TYPE", 0x84 },
	{ "v8dbg_type_InterceptorInfo__INTERCEPTOR_INFO_TYPE", 0x94 },
	{ "v8dbg_type_JSArray__JS_ARRAY_TYPE", 0xa8 },
	{ "v8dbg_type_JSBuiltinsObject__JS_BUILTINS_OBJECT_TYPE", 0xa6 },
	{ "v8dbg_type_JSFunction__JS_FUNCTION_TYPE", 0xac },
	{ "v8dbg_type_JSFunctionProxy__JS_FUNCTION_PROXY_TYPE", 0xad },
	{ "v8dbg_type_JSGlobalObject__JS_GLOBAL_OBJECT_TYPE", 0xa5 },
	{ "v8dbg_type_JSGlobalPropertyCell__JS_GLOBAL_PROPERTY_CELL_TYPE",
		0x83 },
	{ "v8dbg_type_JSMessageObject__JS_MESSAGE_OBJECT_TYPE", 0xa1 },
	{ "v8dbg_type_JSObject__JS_OBJECT_TYPE", 0xa3 },
	{ "v8dbg_type_JSProxy__JS_PROXY_TYPE", 0xa9 },
	{ "v8dbg_type_JSRegExp__JS_REGEXP_TYPE", 0xab },
	{ "v8dbg_type_JSValue__JS_VALUE_TYPE", 0xa2 },
	{ "v8dbg_type_JSWeakMap__JS_WEAK_MAP_TYPE", 0xaa },
	{ "v8dbg_type_Map__MAP_TYPE", 0x80 },
	{ "v8dbg_type_ObjectTemplateInfo__OBJECT_TEMPLATE_INFO_TYPE", 0x97 },
	{ "v8dbg_type_Oddball__ODDBALL_TYPE", 0x82 },
	{ "v8dbg_type_PolymorphicCodeCache__POLYMORPHIC_CODE_CACHE_TYPE",
		0x9c },
	{ "v8dbg_type_Script__SCRIPT_TYPE", 0x9a },
	{ "v8dbg_type_SeqAsciiString__ASCII_STRING_TYPE", 0x4 },
	{ "v8dbg_type_SeqAsciiString__ASCII_SYMBOL_TYPE", 0x44 },
	{ "v8dbg_type_SeqTwoByteString__STRING_TYPE", 0x0 },
	{ "v8dbg_type_SeqTwoByteString__SYMBOL_TYPE", 0x40 },
	{ "v8dbg_type_SharedFunctionInfo__SHARED_FUNCTION_INFO_TYPE", 0xa0 },
	{ "v8dbg_type_SignatureInfo__SIGNATURE_INFO_TYPE", 0x98 },
	{ "v8dbg_type_SlicedString__SLICED_ASCII_STRING_TYPE", 0x7 },
	{ "v8dbg_type_SlicedString__SLICED_STRING_TYPE", 0x3 },
	{ "v8dbg_type_TypeSwitchInfo__TYPE_SWITCH_INFO_TYPE", 0x99 },

	{ "v8dbg_class_AccessCheckInfo__data__Object", 0xc },
	{ "v8dbg_class_AccessCheckInfo__indexed_callback__Object", 0x8 },
	{ "v8dbg_class_AccessCheckInfo__named_callback__Object", 0x4 },
	{ "v8dbg_class_AccessorInfo__data__Object", 0xc },
	{ "v8dbg_class_AccessorInfo__flag__Smi", 0x14 },
	{ "v8dbg_class_AccessorInfo__getter__Object", 0x4 },
	{ "v8dbg_class_AccessorInfo__name__Object", 0x10 },
	{ "v8dbg_class_AccessorInfo__setter__Object", 0x8 },
	{ "v8dbg_class_BreakPointInfo__break_point_objects__Object", 0x10 },
	{ "v8dbg_class_BreakPointInfo__code_position__Smi", 0x4 },
	{ "v8dbg_class_BreakPointInfo__source_position__Smi", 0x8 },
	{ "v8dbg_class_BreakPointInfo__statement_position__Smi", 0xc },
	{ "v8dbg_class_CallHandlerInfo__callback__Object", 0x4 },
	{ "v8dbg_class_CallHandlerInfo__data__Object", 0x8 },
	{ "v8dbg_class_Code__deoptimization_data__FixedArray", 0xc },
	{ "v8dbg_class_Code__instruction_size__int", 0x4 },
	{ "v8dbg_class_Code__instruction_start__int", 0x20 },
	{ "v8dbg_class_Code__next_code_flushing_candidate__Object", 0x10 },
	{ "v8dbg_class_Code__relocation_info__ByteArray", 0x8 },
	{ "v8dbg_class_CodeCache__default_cache__FixedArray", 0x4 },
	{ "v8dbg_class_CodeCache__normal_type_cache__Object", 0x8 },
	{ "v8dbg_class_ConsString__first__String", 0xc },
	{ "v8dbg_class_ConsString__second__String", 0x10 },
	{ "v8dbg_class_DebugInfo__break_points__FixedArray", 0x14 },
	{ "v8dbg_class_DebugInfo__code__Code", 0xc },
	{ "v8dbg_class_DebugInfo__original_code__Code", 0x8 },
	{ "v8dbg_class_DebugInfo__shared__SharedFunctionInfo", 0x4 },
	{ "v8dbg_class_ExternalString__resource__Object", 0xc },
	{ "v8dbg_class_FixedArray__data__uintptr_t", 0x8 },
	{ "v8dbg_class_FixedArrayBase__length__SMI", 0x4 },
	{ "v8dbg_class_FunctionTemplateInfo__access_check_info__Object", 0x38 },
	{ "v8dbg_class_FunctionTemplateInfo__call_code__Object", 0x10 },
	{ "v8dbg_class_FunctionTemplateInfo__class_name__Object", 0x2c },
	{ "v8dbg_class_FunctionTemplateInfo__flag__Smi", 0x3c },
	{ "v8dbg_class_FunctionTemplateInfo__indexed_property_handler__Object",
		0x24 },
	{ "v8dbg_class_FunctionTemplateInfo__instance_call_handler__Object",
		0x34 },
	{ "v8dbg_class_FunctionTemplateInfo__instance_template__Object", 0x28 },
	{ "v8dbg_class_FunctionTemplateInfo__named_property_handler__Object",
		0x20 },
	{ "v8dbg_class_FunctionTemplateInfo__parent_template__Object", 0x1c },
	{ "v8dbg_class_FunctionTemplateInfo__property_accessors__Object",
		0x14 },
	{ "v8dbg_class_FunctionTemplateInfo__prototype_template__Object",
		0x18 },
	{ "v8dbg_class_FunctionTemplateInfo__serial_number__Object", 0xc },
	{ "v8dbg_class_FunctionTemplateInfo__signature__Object", 0x30 },
	{ "v8dbg_class_GlobalObject__builtins__JSBuiltinsObject", 0xc },
	{ "v8dbg_class_GlobalObject__global_context__Context", 0x10 },
	{ "v8dbg_class_GlobalObject__global_receiver__JSObject", 0x14 },
	{ "v8dbg_class_HeapNumber__value__double", 0x4 },
	{ "v8dbg_class_HeapObject__map__Map", 0x0 },
	{ "v8dbg_class_InterceptorInfo__data__Object", 0x18 },
	{ "v8dbg_class_InterceptorInfo__deleter__Object", 0x10 },
	{ "v8dbg_class_InterceptorInfo__enumerator__Object", 0x14 },
	{ "v8dbg_class_InterceptorInfo__getter__Object", 0x4 },
	{ "v8dbg_class_InterceptorInfo__query__Object", 0xc },
	{ "v8dbg_class_InterceptorInfo__setter__Object", 0x8 },
	{ "v8dbg_class_JSArray__length__Object", 0xc },
	{ "v8dbg_class_JSFunction__literals__FixedArray", 0x1c },
	{ "v8dbg_class_JSFunction__next_function_link__Object", 0x20 },
	{ "v8dbg_class_JSFunction__prototype_or_initial_map__Object", 0x10 },
	{ "v8dbg_class_JSFunction__shared__SharedFunctionInfo", 0x14 },
	{ "v8dbg_class_JSFunctionProxy__call_trap__Object", 0x8 },
	{ "v8dbg_class_JSFunctionProxy__construct_trap__Object", 0xc },
	{ "v8dbg_class_JSGlobalProxy__context__Object", 0xc },
	{ "v8dbg_class_JSMessageObject__arguments__JSArray", 0x10 },
	{ "v8dbg_class_JSMessageObject__end_position__SMI", 0x24 },
	{ "v8dbg_class_JSMessageObject__script__Object", 0x14 },
	{ "v8dbg_class_JSMessageObject__stack_frames__Object", 0x1c },
	{ "v8dbg_class_JSMessageObject__stack_trace__Object", 0x18 },
	{ "v8dbg_class_JSMessageObject__start_position__SMI", 0x20 },
	{ "v8dbg_class_JSMessageObject__type__String", 0xc },
	{ "v8dbg_class_JSObject__elements__Object", 0x8 },
	{ "v8dbg_class_JSObject__properties__FixedArray", 0x4 },
	{ "v8dbg_class_JSProxy__handler__Object", 0x4 },
	{ "v8dbg_class_JSRegExp__data__Object", 0xc },
	{ "v8dbg_class_JSValue__value__Object", 0xc },
	{ "v8dbg_class_JSWeakMap__next__Object", 0x10 },
	{ "v8dbg_class_JSWeakMap__table__ObjectHashTable", 0xc },
	{ "v8dbg_class_Map__code_cache__Object", 0x18 },
	{ "v8dbg_class_Map__constructor__Object", 0x10 },
	{ "v8dbg_class_Map__inobject_properties__int", 0x5 },
	{ "v8dbg_class_Map__instance_attributes__int", 0x8 },
	{ "v8dbg_class_Map__instance_descriptors__FixedArray", 0x14 },
	{ "v8dbg_class_Map__instance_size__int", 0x4 },
	{ "v8dbg_class_Map__prototype_transitions__FixedArray", 0x1c },
	{ "v8dbg_class_ObjectTemplateInfo__constructor__Object", 0xc },
	{ "v8dbg_class_ObjectTemplateInfo__internal_field_count__Object",
		0x10 },
	{ "v8dbg_class_Oddball__to_number__Object", 0x8 },
	{ "v8dbg_class_Oddball__to_string__String", 0x4 },
	{ "v8dbg_class_PolymorphicCodeCache__cache__Object", 0x4 },
	{ "v8dbg_class_Script__column_offset__Smi", 0x10 },
	{ "v8dbg_class_Script__compilation_type__Smi", 0x24 },
	{ "v8dbg_class_Script__context_data__Object", 0x18 },
	{ "v8dbg_class_Script__data__Object", 0x14 },
	{ "v8dbg_class_Script__eval_from_instructions_offset__Smi", 0x34 },
	{ "v8dbg_class_Script__eval_from_shared__Object", 0x30 },
	{ "v8dbg_class_Script__id__Object", 0x2c },
	{ "v8dbg_class_Script__line_ends__Object", 0x28 },
	{ "v8dbg_class_Script__line_offset__Smi", 0xc },
	{ "v8dbg_class_Script__name__Object", 0x8 },
	{ "v8dbg_class_Script__source__Object", 0x4 },
	{ "v8dbg_class_Script__type__Smi", 0x20 },
	{ "v8dbg_class_Script__wrapper__Foreign", 0x1c },
	{ "v8dbg_class_SeqAsciiString__chars__char", 0xc },
	{ "v8dbg_class_SharedFunctionInfo__code__Code", 0x8 },
	{ "v8dbg_class_SharedFunctionInfo__compiler_hints__SMI", 0x50 },
	{ "v8dbg_class_SharedFunctionInfo__construct_stub__Code", 0x10 },
	{ "v8dbg_class_SharedFunctionInfo__debug_info__Object", 0x20 },
	{ "v8dbg_class_SharedFunctionInfo__end_position__SMI", 0x48 },
	{ "v8dbg_class_SharedFunctionInfo__expected_nof_properties__SMI",
		0x3c },
	{ "v8dbg_class_SharedFunctionInfo__formal_parameter_count__SMI", 0x38 },
	{ "v8dbg_class_SharedFunctionInfo__function_data__Object", 0x18 },
	{ "v8dbg_class_SharedFunctionInfo__function_token_position__SMI",
		0x4c },
	{ "v8dbg_class_SharedFunctionInfo__inferred_name__String", 0x24 },
	{ "v8dbg_class_SharedFunctionInfo__initial_map__Object", 0x28 },
	{ "v8dbg_class_SharedFunctionInfo__instance_class_name__Object", 0x14 },
	{ "v8dbg_class_SharedFunctionInfo__length__SMI", 0x34 },
	{ "v8dbg_class_SharedFunctionInfo__name__Object", 0x4 },
	{ "v8dbg_class_SharedFunctionInfo__num_literals__SMI", 0x40 },
	{ "v8dbg_class_SharedFunctionInfo__opt_count__SMI", 0x58 },
	{ "v8dbg_class_SharedFunctionInfo__script__Object", 0x1c },
	{ "v8dbg_class_SharedFunctionInfo__"
		"start_position_and_type__SMI", 0x44 },
	{ "v8dbg_class_SharedFunctionInfo__"
		"this_property_assignments__Object", 0x2c },
	{ "v8dbg_class_SharedFunctionInfo__"
		"this_property_assignments_count__SMI", 0x54 },
	{ "v8dbg_class_SignatureInfo__args__Object", 0x8 },
	{ "v8dbg_class_SignatureInfo__receiver__Object", 0x4 },
	{ "v8dbg_class_SlicedString__offset__SMI", 0x10 },
	{ "v8dbg_class_String__length__SMI", 0x4 },
	{ "v8dbg_class_TemplateInfo__property_list__Object", 0x8 },
	{ "v8dbg_class_TemplateInfo__tag__Object", 0x4 },
	{ "v8dbg_class_TypeSwitchInfo__types__Object", 0x4 },

	{ "v8dbg_parent_AccessCheckInfo__Struct", 0x0 },
	{ "v8dbg_parent_AccessorInfo__Struct", 0x0 },
	{ "v8dbg_parent_BreakPointInfo__Struct", 0x0 },
	{ "v8dbg_parent_ByteArray__FixedArrayBase", 0x0 },
	{ "v8dbg_parent_CallHandlerInfo__Struct", 0x0 },
	{ "v8dbg_parent_Code__HeapObject", 0x0 },
	{ "v8dbg_parent_CodeCache__Struct", 0x0 },
	{ "v8dbg_parent_ConsString__String", 0x0 },
	{ "v8dbg_parent_DebugInfo__Struct", 0x0 },
	{ "v8dbg_parent_DeoptimizationInputData__FixedArray", 0x0 },
	{ "v8dbg_parent_DeoptimizationOutputData__FixedArray", 0x0 },
	{ "v8dbg_parent_DescriptorArray__FixedArray", 0x0 },
	{ "v8dbg_parent_ExternalArray__FixedArrayBase", 0x0 },
	{ "v8dbg_parent_ExternalAsciiString__ExternalString", 0x0 },
	{ "v8dbg_parent_ExternalByteArray__ExternalArray", 0x0 },
	{ "v8dbg_parent_ExternalDoubleArray__ExternalArray", 0x0 },
	{ "v8dbg_parent_ExternalFloatArray__ExternalArray", 0x0 },
	{ "v8dbg_parent_ExternalIntArray__ExternalArray", 0x0 },
	{ "v8dbg_parent_ExternalPixelArray__ExternalArray", 0x0 },
	{ "v8dbg_parent_ExternalShortArray__ExternalArray", 0x0 },
	{ "v8dbg_parent_ExternalString__String", 0x0 },
	{ "v8dbg_parent_ExternalTwoByteString__ExternalString", 0x0 },
	{ "v8dbg_parent_ExternalUnsignedByteArray__ExternalArray", 0x0 },
	{ "v8dbg_parent_ExternalUnsignedIntArray__ExternalArray", 0x0 },
	{ "v8dbg_parent_ExternalUnsignedShortArray__ExternalArray", 0x0 },
	{ "v8dbg_parent_Failure__MaybeObject", 0x0 },
	{ "v8dbg_parent_FixedArray__FixedArrayBase", 0x0 },
	{ "v8dbg_parent_FixedArrayBase__HeapObject", 0x0 },
	{ "v8dbg_parent_FixedDoubleArray__FixedArrayBase", 0x0 },
	{ "v8dbg_parent_Foreign__HeapObject", 0x0 },
	{ "v8dbg_parent_FunctionTemplateInfo__TemplateInfo", 0x0 },
	{ "v8dbg_parent_GlobalObject__JSObject", 0x0 },
	{ "v8dbg_parent_HashTable__FixedArray", 0x0 },
	{ "v8dbg_parent_HeapNumber__HeapObject", 0x0 },
	{ "v8dbg_parent_HeapObject__Object", 0x0 },
	{ "v8dbg_parent_InterceptorInfo__Struct", 0x0 },
	{ "v8dbg_parent_JSArray__JSObject", 0x0 },
	{ "v8dbg_parent_JSBuiltinsObject__GlobalObject", 0x0 },
	{ "v8dbg_parent_JSFunction__JSObject", 0x0 },
	{ "v8dbg_parent_JSFunctionProxy__JSProxy", 0x0 },
	{ "v8dbg_parent_JSFunctionResultCache__FixedArray", 0x0 },
	{ "v8dbg_parent_JSGlobalObject__GlobalObject", 0x0 },
	{ "v8dbg_parent_JSGlobalPropertyCell__HeapObject", 0x0 },
	{ "v8dbg_parent_JSMessageObject__JSObject", 0x0 },
	{ "v8dbg_parent_JSObject__JSReceiver", 0x0 },
	{ "v8dbg_parent_JSProxy__JSReceiver", 0x0 },
	{ "v8dbg_parent_JSReceiver__HeapObject", 0x0 },
	{ "v8dbg_parent_JSRegExp__JSObject", 0x0 },
	{ "v8dbg_parent_JSRegExpResult__JSArray", 0x0 },
	{ "v8dbg_parent_JSValue__JSObject", 0x0 },
	{ "v8dbg_parent_JSWeakMap__JSObject", 0x0 },
	{ "v8dbg_parent_Map__HeapObject", 0x0 },
	{ "v8dbg_parent_NormalizedMapCache__FixedArray", 0x0 },
	{ "v8dbg_parent_ObjectTemplateInfo__TemplateInfo", 0x0 },
	{ "v8dbg_parent_Oddball__HeapObject", 0x0 },
	{ "v8dbg_parent_PolymorphicCodeCache__Struct", 0x0 },
	{ "v8dbg_parent_Script__Struct", 0x0 },
	{ "v8dbg_parent_SeqAsciiString__SeqString", 0x0 },
	{ "v8dbg_parent_SeqString__String", 0x0 },
	{ "v8dbg_parent_SeqTwoByteString__SeqString", 0x0 },
	{ "v8dbg_parent_SharedFunctionInfo__HeapObject", 0x0 },
	{ "v8dbg_parent_SignatureInfo__Struct", 0x0 },
	{ "v8dbg_parent_SlicedString__String", 0x0 },
	{ "v8dbg_parent_Smi__Object", 0x0 },
	{ "v8dbg_parent_String__HeapObject", 0x0 },
	{ "v8dbg_parent_Struct__HeapObject", 0x0 },
	{ "v8dbg_parent_TemplateInfo__Struct", 0x0 },
	{ "v8dbg_parent_TypeSwitchInfo__Struct", 0x0 },

	{ "v8dbg_frametype_ArgumentsAdaptorFrame", 0x8 },
	{ "v8dbg_frametype_ConstructFrame", 0x7 },
	{ "v8dbg_frametype_EntryConstructFrame", 0x2 },
	{ "v8dbg_frametype_EntryFrame", 0x1 },
	{ "v8dbg_frametype_ExitFrame", 0x3 },
	{ "v8dbg_frametype_InternalFrame", 0x6 },
	{ "v8dbg_frametype_JavaScriptFrame", 0x4 },
	{ "v8dbg_frametype_OptimizedFrame", 0x5 },

	{ "v8dbg_off_fp_args", 0x8 },
	{ "v8dbg_off_fp_context", -0x4 },
	{ "v8dbg_off_fp_function", -0x8 },
	{ "v8dbg_off_fp_marker", -0x8 },

	{ "v8dbg_prop_idx_content", 0x1 },
	{ "v8dbg_prop_idx_first", 0x3 },
	{ "v8dbg_prop_type_field", 0x1 },
	{ "v8dbg_prop_type_first_phantom", 0x6 },
	{ "v8dbg_prop_type_mask", 0xf },

	{ "v8dbg_AsciiStringTag", 0x4 },
	{ "v8dbg_PointerSizeLog2", 0x2 },
	{ "v8dbg_SeqStringTag", 0x0 },
	{ "v8dbg_SmiTag", 0x0 },
	{ "v8dbg_SmiTagMask", 0x1 },
	{ "v8dbg_SmiValueShift", 0x1 },
	{ "v8dbg_StringEncodingMask", 0x4 },
	{ "v8dbg_StringRepresentationMask", 0x3 },
	{ "v8dbg_StringTag", 0x0 },
	{ "v8dbg_TwoByteStringTag", 0x0 },
	{ "v8dbg_ConsStringTag", 0x1 },
	{ "v8dbg_ExternalStringTag", 0x2 },
	{ "v8dbg_FailureTag", 0x3 },
	{ "v8dbg_FailureTagMask", 0x3 },
	{ "v8dbg_FirstNonstringType", 0x80 },
	{ "v8dbg_HeapObjectTag", 0x1 },
	{ "v8dbg_HeapObjectTagMask", 0x3 },
	{ "v8dbg_IsNotStringMask", 0x80 },
	{ "v8dbg_NotStringTag", 0x80 },

	{ NULL },
};

v8_cfg_t v8_cfg_04 = { "node-0.4", "node v0.4", v8_symbols_node_04,
    v8cfg_canned_iter, v8cfg_canned_readsym };

v8_cfg_t v8_cfg_06 = { "node-0.6", "node v0.6", v8_symbols_node_06,
    v8cfg_canned_iter, v8cfg_canned_readsym };

v8_cfg_t *v8_cfgs[] = {
	&v8_cfg_04,
	&v8_cfg_06,
	NULL
};

v8_cfg_t v8_cfg_target = { NULL, NULL, NULL, v8cfg_target_iter,
	v8cfg_target_readsym };
