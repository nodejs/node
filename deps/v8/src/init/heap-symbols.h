// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INIT_HEAP_SYMBOLS_H_
#define V8_INIT_HEAP_SYMBOLS_H_

#ifdef V8_INTL_SUPPORT
#define INTERNALIZED_STRING_LIST_GENERATOR_INTL(V, _)               \
  V(_, adoptText_string, "adoptText")                               \
  V(_, baseName_string, "baseName")                                 \
  V(_, accounting_string, "accounting")                             \
  V(_, breakType_string, "breakType")                               \
  V(_, calendar_string, "calendar")                                 \
  V(_, cardinal_string, "cardinal")                                 \
  V(_, caseFirst_string, "caseFirst")                               \
  V(_, compare_string, "compare")                                   \
  V(_, collation_string, "collation")                               \
  V(_, compact_string, "compact")                                   \
  V(_, compactDisplay_string, "compactDisplay")                     \
  V(_, currency_string, "currency")                                 \
  V(_, currencyDisplay_string, "currencyDisplay")                   \
  V(_, currencySign_string, "currencySign")                         \
  V(_, dateStyle_string, "dateStyle")                               \
  V(_, dateTimeField_string, "dateTimeField")                       \
  V(_, day_string, "day")                                           \
  V(_, dayPeriod_string, "dayPeriod")                               \
  V(_, decimal_string, "decimal")                                   \
  V(_, endRange_string, "endRange")                                 \
  V(_, engineering_string, "engineering")                           \
  V(_, era_string, "era")                                           \
  V(_, exceptZero_string, "exceptZero")                             \
  V(_, exponentInteger_string, "exponentInteger")                   \
  V(_, exponentMinusSign_string, "exponentMinusSign")               \
  V(_, exponentSeparator_string, "exponentSeparator")               \
  V(_, fallback_string, "fallback")                                 \
  V(_, first_string, "first")                                       \
  V(_, format_string, "format")                                     \
  V(_, fraction_string, "fraction")                                 \
  V(_, fractionalSecond_string, "fractionalSecond")                 \
  V(_, fractionalSecondDigits_string, "fractionalSecondDigits")     \
  V(_, full_string, "full")                                         \
  V(_, granularity_string, "granularity")                           \
  V(_, grapheme_string, "grapheme")                                 \
  V(_, group_string, "group")                                       \
  V(_, h11_string, "h11")                                           \
  V(_, h12_string, "h12")                                           \
  V(_, h23_string, "h23")                                           \
  V(_, h24_string, "h24")                                           \
  V(_, hour_string, "hour")                                         \
  V(_, hour12_string, "hour12")                                     \
  V(_, hourCycle_string, "hourCycle")                               \
  V(_, ideo_string, "ideo")                                         \
  V(_, ignorePunctuation_string, "ignorePunctuation")               \
  V(_, Invalid_Date_string, "Invalid Date")                         \
  V(_, integer_string, "integer")                                   \
  V(_, kana_string, "kana")                                         \
  V(_, language_string, "language")                                 \
  V(_, letter_string, "letter")                                     \
  V(_, list_string, "list")                                         \
  V(_, literal_string, "literal")                                   \
  V(_, locale_string, "locale")                                     \
  V(_, loose_string, "loose")                                       \
  V(_, lower_string, "lower")                                       \
  V(_, maximumFractionDigits_string, "maximumFractionDigits")       \
  V(_, maximumSignificantDigits_string, "maximumSignificantDigits") \
  V(_, minimumFractionDigits_string, "minimumFractionDigits")       \
  V(_, minimumIntegerDigits_string, "minimumIntegerDigits")         \
  V(_, minimumSignificantDigits_string, "minimumSignificantDigits") \
  V(_, minusSign_string, "minusSign")                               \
  V(_, minute_string, "minute")                                     \
  V(_, month_string, "month")                                       \
  V(_, nan_string, "nan")                                           \
  V(_, narrowSymbol_string, "narrowSymbol")                         \
  V(_, never_string, "never")                                       \
  V(_, none_string, "none")                                         \
  V(_, notation_string, "notation")                                 \
  V(_, normal_string, "normal")                                     \
  V(_, numberingSystem_string, "numberingSystem")                   \
  V(_, numeric_string, "numeric")                                   \
  V(_, ordinal_string, "ordinal")                                   \
  V(_, percentSign_string, "percentSign")                           \
  V(_, plusSign_string, "plusSign")                                 \
  V(_, quarter_string, "quarter")                                   \
  V(_, region_string, "region")                                     \
  V(_, relatedYear_string, "relatedYear")                           \
  V(_, scientific_string, "scientific")                             \
  V(_, second_string, "second")                                     \
  V(_, segment_string, "segment")                                   \
  V(_, SegmentIterator_string, "Segment Iterator")                  \
  V(_, sensitivity_string, "sensitivity")                           \
  V(_, sep_string, "sep")                                           \
  V(_, shared_string, "shared")                                     \
  V(_, signDisplay_string, "signDisplay")                           \
  V(_, standard_string, "standard")                                 \
  V(_, startRange_string, "startRange")                             \
  V(_, strict_string, "strict")                                     \
  V(_, style_string, "style")                                       \
  V(_, term_string, "term")                                         \
  V(_, timeStyle_string, "timeStyle")                               \
  V(_, timeZone_string, "timeZone")                                 \
  V(_, timeZoneName_string, "timeZoneName")                         \
  V(_, type_string, "type")                                         \
  V(_, unknown_string, "unknown")                                   \
  V(_, upper_string, "upper")                                       \
  V(_, usage_string, "usage")                                       \
  V(_, useGrouping_string, "useGrouping")                           \
  V(_, UTC_string, "UTC")                                           \
  V(_, unit_string, "unit")                                         \
  V(_, unitDisplay_string, "unitDisplay")                           \
  V(_, weekday_string, "weekday")                                   \
  V(_, year_string, "year")                                         \
  V(_, yearName_string, "yearName")
#else  // V8_INTL_SUPPORT
#define INTERNALIZED_STRING_LIST_GENERATOR_INTL(V, _)
#endif  // V8_INTL_SUPPORT

#define INTERNALIZED_STRING_LIST_GENERATOR(V, _)                     \
  INTERNALIZED_STRING_LIST_GENERATOR_INTL(V, _)                      \
  V(_, add_string, "add")                                            \
  V(_, always_string, "always")                                      \
  V(_, anonymous_function_string, "(anonymous function)")            \
  V(_, anonymous_string, "anonymous")                                \
  V(_, apply_string, "apply")                                        \
  V(_, Arguments_string, "Arguments")                                \
  V(_, arguments_string, "arguments")                                \
  V(_, arguments_to_string, "[object Arguments]")                    \
  V(_, Array_string, "Array")                                        \
  V(_, array_to_string, "[object Array]")                            \
  V(_, ArrayBuffer_string, "ArrayBuffer")                            \
  V(_, ArrayIterator_string, "Array Iterator")                       \
  V(_, as_string, "as")                                              \
  V(_, async_string, "async")                                        \
  V(_, auto_string, "auto")                                          \
  V(_, await_string, "await")                                        \
  V(_, BigInt_string, "BigInt")                                      \
  V(_, bigint_string, "bigint")                                      \
  V(_, BigInt64Array_string, "BigInt64Array")                        \
  V(_, BigUint64Array_string, "BigUint64Array")                      \
  V(_, bind_string, "bind")                                          \
  V(_, Boolean_string, "Boolean")                                    \
  V(_, boolean_string, "boolean")                                    \
  V(_, boolean_to_string, "[object Boolean]")                        \
  V(_, bound__string, "bound ")                                      \
  V(_, buffer_string, "buffer")                                      \
  V(_, byte_length_string, "byteLength")                             \
  V(_, byte_offset_string, "byteOffset")                             \
  V(_, CompileError_string, "CompileError")                          \
  V(_, callee_string, "callee")                                      \
  V(_, caller_string, "caller")                                      \
  V(_, character_string, "character")                                \
  V(_, closure_string, "(closure)")                                  \
  V(_, code_string, "code")                                          \
  V(_, column_string, "column")                                      \
  V(_, computed_string, "<computed>")                                \
  V(_, configurable_string, "configurable")                          \
  V(_, conjunction_string, "conjunction")                            \
  V(_, construct_string, "construct")                                \
  V(_, constructor_string, "constructor")                            \
  V(_, current_string, "current")                                    \
  V(_, Date_string, "Date")                                          \
  V(_, date_to_string, "[object Date]")                              \
  V(_, default_string, "default")                                    \
  V(_, defineProperty_string, "defineProperty")                      \
  V(_, deleteProperty_string, "deleteProperty")                      \
  V(_, disjunction_string, "disjunction")                            \
  V(_, display_name_string, "displayName")                           \
  V(_, done_string, "done")                                          \
  V(_, dot_brand_string, ".brand")                                   \
  V(_, dot_catch_string, ".catch")                                   \
  V(_, dot_default_string, ".default")                               \
  V(_, dot_for_string, ".for")                                       \
  V(_, dot_generator_object_string, ".generator_object")             \
  V(_, dot_result_string, ".result")                                 \
  V(_, dot_repl_result_string, ".repl_result")                       \
  V(_, dot_string, ".")                                              \
  V(_, dot_switch_tag_string, ".switch_tag")                         \
  V(_, dotAll_string, "dotAll")                                      \
  V(_, enumerable_string, "enumerable")                              \
  V(_, element_string, "element")                                    \
  V(_, Error_string, "Error")                                        \
  V(_, error_to_string, "[object Error]")                            \
  V(_, eval_string, "eval")                                          \
  V(_, EvalError_string, "EvalError")                                \
  V(_, exec_string, "exec")                                          \
  V(_, false_string, "false")                                        \
  V(_, flags_string, "flags")                                        \
  V(_, Float32Array_string, "Float32Array")                          \
  V(_, Float64Array_string, "Float64Array")                          \
  V(_, from_string, "from")                                          \
  V(_, Function_string, "Function")                                  \
  V(_, function_native_code_string, "function () { [native code] }") \
  V(_, function_string, "function")                                  \
  V(_, function_to_string, "[object Function]")                      \
  V(_, Generator_string, "Generator")                                \
  V(_, get_space_string, "get ")                                     \
  V(_, get_string, "get")                                            \
  V(_, getOwnPropertyDescriptor_string, "getOwnPropertyDescriptor")  \
  V(_, getPrototypeOf_string, "getPrototypeOf")                      \
  V(_, global_string, "global")                                      \
  V(_, globalThis_string, "globalThis")                              \
  V(_, groups_string, "groups")                                      \
  V(_, has_string, "has")                                            \
  V(_, ignoreCase_string, "ignoreCase")                              \
  V(_, illegal_access_string, "illegal access")                      \
  V(_, illegal_argument_string, "illegal argument")                  \
  V(_, index_string, "index")                                        \
  V(_, indices_string, "indices")                                    \
  V(_, Infinity_string, "Infinity")                                  \
  V(_, infinity_string, "infinity")                                  \
  V(_, input_string, "input")                                        \
  V(_, Int16Array_string, "Int16Array")                              \
  V(_, Int32Array_string, "Int32Array")                              \
  V(_, Int8Array_string, "Int8Array")                                \
  V(_, isExtensible_string, "isExtensible")                          \
  V(_, jsMemoryEstimate_string, "jsMemoryEstimate")                  \
  V(_, jsMemoryRange_string, "jsMemoryRange")                        \
  V(_, keys_string, "keys")                                          \
  V(_, lastIndex_string, "lastIndex")                                \
  V(_, length_string, "length")                                      \
  V(_, let_string, "let")                                            \
  V(_, line_string, "line")                                          \
  V(_, LinkError_string, "LinkError")                                \
  V(_, long_string, "long")                                          \
  V(_, Map_string, "Map")                                            \
  V(_, MapIterator_string, "Map Iterator")                           \
  V(_, medium_string, "medium")                                      \
  V(_, message_string, "message")                                    \
  V(_, meta_string, "meta")                                          \
  V(_, minus_Infinity_string, "-Infinity")                           \
  V(_, Module_string, "Module")                                      \
  V(_, multiline_string, "multiline")                                \
  V(_, name_string, "name")                                          \
  V(_, NaN_string, "NaN")                                            \
  V(_, narrow_string, "narrow")                                      \
  V(_, native_string, "native")                                      \
  V(_, new_target_string, ".new.target")                             \
  V(_, next_string, "next")                                          \
  V(_, NFC_string, "NFC")                                            \
  V(_, NFD_string, "NFD")                                            \
  V(_, NFKC_string, "NFKC")                                          \
  V(_, NFKD_string, "NFKD")                                          \
  V(_, not_equal, "not-equal")                                       \
  V(_, null_string, "null")                                          \
  V(_, null_to_string, "[object Null]")                              \
  V(_, Number_string, "Number")                                      \
  V(_, number_string, "number")                                      \
  V(_, number_to_string, "[object Number]")                          \
  V(_, Object_string, "Object")                                      \
  V(_, object_string, "object")                                      \
  V(_, object_to_string, "[object Object]")                          \
  V(_, of_string, "of")                                              \
  V(_, ok, "ok")                                                     \
  V(_, one_string, "1")                                              \
  V(_, other_string, "other")                                        \
  V(_, ownKeys_string, "ownKeys")                                    \
  V(_, percent_string, "percent")                                    \
  V(_, position_string, "position")                                  \
  V(_, preventExtensions_string, "preventExtensions")                \
  V(_, private_constructor_string, "#constructor")                   \
  V(_, Promise_string, "Promise")                                    \
  V(_, proto_string, "__proto__")                                    \
  V(_, prototype_string, "prototype")                                \
  V(_, proxy_string, "proxy")                                        \
  V(_, Proxy_string, "Proxy")                                        \
  V(_, query_colon_string, "(?:)")                                   \
  V(_, RangeError_string, "RangeError")                              \
  V(_, raw_string, "raw")                                            \
  V(_, ReferenceError_string, "ReferenceError")                      \
  V(_, ReflectGet_string, "Reflect.get")                             \
  V(_, ReflectHas_string, "Reflect.has")                             \
  V(_, RegExp_string, "RegExp")                                      \
  V(_, regexp_to_string, "[object RegExp]")                          \
  V(_, resolve_string, "resolve")                                    \
  V(_, return_string, "return")                                      \
  V(_, revoke_string, "revoke")                                      \
  V(_, RuntimeError_string, "RuntimeError")                          \
  V(_, Script_string, "Script")                                      \
  V(_, script_string, "script")                                      \
  V(_, short_string, "short")                                        \
  V(_, Set_string, "Set")                                            \
  V(_, sentence_string, "sentence")                                  \
  V(_, set_space_string, "set ")                                     \
  V(_, set_string, "set")                                            \
  V(_, SetIterator_string, "Set Iterator")                           \
  V(_, setPrototypeOf_string, "setPrototypeOf")                      \
  V(_, SharedArrayBuffer_string, "SharedArrayBuffer")                \
  V(_, source_string, "source")                                      \
  V(_, sourceText_string, "sourceText")                              \
  V(_, stack_string, "stack")                                        \
  V(_, stackTraceLimit_string, "stackTraceLimit")                    \
  V(_, sticky_string, "sticky")                                      \
  V(_, String_string, "String")                                      \
  V(_, string_string, "string")                                      \
  V(_, string_to_string, "[object String]")                          \
  V(_, symbol_species_string, "[Symbol.species]")                    \
  V(_, Symbol_string, "Symbol")                                      \
  V(_, symbol_string, "symbol")                                      \
  V(_, SyntaxError_string, "SyntaxError")                            \
  V(_, target_string, "target")                                      \
  V(_, then_string, "then")                                          \
  V(_, this_function_string, ".this_function")                       \
  V(_, this_string, "this")                                          \
  V(_, throw_string, "throw")                                        \
  V(_, timed_out, "timed-out")                                       \
  V(_, toJSON_string, "toJSON")                                      \
  V(_, toString_string, "toString")                                  \
  V(_, true_string, "true")                                          \
  V(_, total_string, "total")                                        \
  V(_, TypeError_string, "TypeError")                                \
  V(_, Uint16Array_string, "Uint16Array")                            \
  V(_, Uint32Array_string, "Uint32Array")                            \
  V(_, Uint8Array_string, "Uint8Array")                              \
  V(_, Uint8ClampedArray_string, "Uint8ClampedArray")                \
  V(_, undefined_string, "undefined")                                \
  V(_, undefined_to_string, "[object Undefined]")                    \
  V(_, unicode_string, "unicode")                                    \
  V(_, URIError_string, "URIError")                                  \
  V(_, value_string, "value")                                        \
  V(_, valueOf_string, "valueOf")                                    \
  V(_, WeakMap_string, "WeakMap")                                    \
  V(_, WeakRef_string, "WeakRef")                                    \
  V(_, WeakSet_string, "WeakSet")                                    \
  V(_, week_string, "week")                                          \
  V(_, word_string, "word")                                          \
  V(_, writable_string, "writable")                                  \
  V(_, zero_string, "0")

#define PRIVATE_SYMBOL_LIST_GENERATOR(V, _)           \
  V(_, call_site_frame_array_symbol)                  \
  V(_, call_site_frame_index_symbol)                  \
  V(_, console_context_id_symbol)                     \
  V(_, console_context_name_symbol)                   \
  V(_, class_fields_symbol)                           \
  V(_, class_positions_symbol)                        \
  V(_, detailed_stack_trace_symbol)                   \
  V(_, elements_transition_symbol)                    \
  V(_, error_end_pos_symbol)                          \
  V(_, error_script_symbol)                           \
  V(_, error_start_pos_symbol)                        \
  V(_, frozen_symbol)                                 \
  V(_, generic_symbol)                                \
  V(_, home_object_symbol)                            \
  V(_, interpreter_trampoline_symbol)                 \
  V(_, megamorphic_symbol)                            \
  V(_, native_context_index_symbol)                   \
  V(_, nonextensible_symbol)                          \
  V(_, not_mapped_symbol)                             \
  V(_, promise_debug_marker_symbol)                   \
  V(_, promise_debug_message_symbol)                  \
  V(_, promise_forwarding_handler_symbol)             \
  V(_, promise_handled_by_symbol)                     \
  V(_, regexp_result_cached_indices_or_regexp_symbol) \
  V(_, regexp_result_names_symbol)                    \
  V(_, regexp_result_regexp_input_symbol)             \
  V(_, regexp_result_regexp_last_index_symbol)        \
  V(_, sealed_symbol)                                 \
  V(_, stack_trace_symbol)                            \
  V(_, strict_function_transition_symbol)             \
  V(_, wasm_exception_tag_symbol)                     \
  V(_, wasm_exception_values_symbol)                  \
  V(_, wasm_uncatchable_symbol)                       \
  V(_, uninitialized_symbol)

#define PUBLIC_SYMBOL_LIST_GENERATOR(V, _)          \
  V(_, async_iterator_symbol, Symbol.asyncIterator) \
  V(_, iterator_symbol, Symbol.iterator)            \
  V(_, intl_fallback_symbol, IntlFallback)          \
  V(_, match_all_symbol, Symbol.matchAll)           \
  V(_, match_symbol, Symbol.match)                  \
  V(_, replace_symbol, Symbol.replace)              \
  V(_, search_symbol, Symbol.search)                \
  V(_, species_symbol, Symbol.species)              \
  V(_, split_symbol, Symbol.split)                  \
  V(_, to_primitive_symbol, Symbol.toPrimitive)     \
  V(_, unscopables_symbol, Symbol.unscopables)

// Well-Known Symbols are "Public" symbols, which have a bit set which causes
// them to produce an undefined value when a load results in a failed access
// check. Because this behaviour is not specified properly as of yet, it only
// applies to a subset of spec-defined Well-Known Symbols.
#define WELL_KNOWN_SYMBOL_LIST_GENERATOR(V, _)                 \
  V(_, has_instance_symbol, Symbol.hasInstance)                \
  V(_, is_concat_spreadable_symbol, Symbol.isConcatSpreadable) \
  V(_, to_string_tag_symbol, Symbol.toStringTag)

#define INCREMENTAL_SCOPES(F)                                      \
  /* MC_INCREMENTAL is the top-level incremental marking scope. */ \
  F(MC_INCREMENTAL)                                                \
  F(MC_INCREMENTAL_EMBEDDER_PROLOGUE)                              \
  F(MC_INCREMENTAL_EMBEDDER_TRACING)                               \
  F(MC_INCREMENTAL_EXTERNAL_EPILOGUE)                              \
  F(MC_INCREMENTAL_EXTERNAL_PROLOGUE)                              \
  F(MC_INCREMENTAL_FINALIZE)                                       \
  F(MC_INCREMENTAL_FINALIZE_BODY)                                  \
  F(MC_INCREMENTAL_LAYOUT_CHANGE)                                  \
  F(MC_INCREMENTAL_START)                                          \
  F(MC_INCREMENTAL_SWEEP_ARRAY_BUFFERS)                            \
  F(MC_INCREMENTAL_SWEEPING)

#define TOP_MC_SCOPES(F) \
  F(MC_CLEAR)            \
  F(MC_EPILOGUE)         \
  F(MC_EVACUATE)         \
  F(MC_FINISH)           \
  F(MC_MARK)             \
  F(MC_PROLOGUE)         \
  F(MC_SWEEP)

#define TRACER_SCOPES(F)                             \
  INCREMENTAL_SCOPES(F)                              \
  F(HEAP_EMBEDDER_TRACING_EPILOGUE)                  \
  F(HEAP_EPILOGUE)                                   \
  F(HEAP_EPILOGUE_REDUCE_NEW_SPACE)                  \
  F(HEAP_EXTERNAL_EPILOGUE)                          \
  F(HEAP_EXTERNAL_PROLOGUE)                          \
  F(HEAP_EXTERNAL_WEAK_GLOBAL_HANDLES)               \
  F(HEAP_PROLOGUE)                                   \
  TOP_MC_SCOPES(F)                                   \
  F(MC_CLEAR_DEPENDENT_CODE)                         \
  F(MC_CLEAR_FLUSHABLE_BYTECODE)                     \
  F(MC_CLEAR_FLUSHED_JS_FUNCTIONS)                   \
  F(MC_CLEAR_MAPS)                                   \
  F(MC_CLEAR_SLOTS_BUFFER)                           \
  F(MC_CLEAR_STORE_BUFFER)                           \
  F(MC_CLEAR_STRING_TABLE)                           \
  F(MC_CLEAR_WEAK_COLLECTIONS)                       \
  F(MC_CLEAR_WEAK_LISTS)                             \
  F(MC_CLEAR_WEAK_REFERENCES)                        \
  F(MC_COMPLETE_SWEEP_ARRAY_BUFFERS)                 \
  F(MC_EVACUATE_CANDIDATES)                          \
  F(MC_EVACUATE_CLEAN_UP)                            \
  F(MC_EVACUATE_COPY)                                \
  F(MC_EVACUATE_COPY_PARALLEL)                       \
  F(MC_EVACUATE_EPILOGUE)                            \
  F(MC_EVACUATE_PROLOGUE)                            \
  F(MC_EVACUATE_REBALANCE)                           \
  F(MC_EVACUATE_UPDATE_POINTERS)                     \
  F(MC_EVACUATE_UPDATE_POINTERS_PARALLEL)            \
  F(MC_EVACUATE_UPDATE_POINTERS_SLOTS_MAIN)          \
  F(MC_EVACUATE_UPDATE_POINTERS_SLOTS_MAP_SPACE)     \
  F(MC_EVACUATE_UPDATE_POINTERS_TO_NEW_ROOTS)        \
  F(MC_EVACUATE_UPDATE_POINTERS_WEAK)                \
  F(MC_FINISH_SWEEP_ARRAY_BUFFERS)                   \
  F(MC_MARK_EMBEDDER_PROLOGUE)                       \
  F(MC_MARK_EMBEDDER_TRACING)                        \
  F(MC_MARK_EMBEDDER_TRACING_CLOSURE)                \
  F(MC_MARK_FINISH_INCREMENTAL)                      \
  F(MC_MARK_MAIN)                                    \
  F(MC_MARK_ROOTS)                                   \
  F(MC_MARK_WEAK_CLOSURE)                            \
  F(MC_MARK_WEAK_CLOSURE_EPHEMERON)                  \
  F(MC_MARK_WEAK_CLOSURE_EPHEMERON_MARKING)          \
  F(MC_MARK_WEAK_CLOSURE_EPHEMERON_LINEAR)           \
  F(MC_MARK_WEAK_CLOSURE_WEAK_HANDLES)               \
  F(MC_MARK_WEAK_CLOSURE_WEAK_ROOTS)                 \
  F(MC_MARK_WEAK_CLOSURE_HARMONY)                    \
  F(MC_SWEEP_CODE)                                   \
  F(MC_SWEEP_MAP)                                    \
  F(MC_SWEEP_OLD)                                    \
  F(MINOR_MC)                                        \
  F(MINOR_MC_CLEAR)                                  \
  F(MINOR_MC_CLEAR_STRING_TABLE)                     \
  F(MINOR_MC_CLEAR_WEAK_LISTS)                       \
  F(MINOR_MC_EVACUATE)                               \
  F(MINOR_MC_EVACUATE_CLEAN_UP)                      \
  F(MINOR_MC_EVACUATE_COPY)                          \
  F(MINOR_MC_EVACUATE_COPY_PARALLEL)                 \
  F(MINOR_MC_EVACUATE_EPILOGUE)                      \
  F(MINOR_MC_EVACUATE_PROLOGUE)                      \
  F(MINOR_MC_EVACUATE_REBALANCE)                     \
  F(MINOR_MC_EVACUATE_UPDATE_POINTERS)               \
  F(MINOR_MC_EVACUATE_UPDATE_POINTERS_PARALLEL)      \
  F(MINOR_MC_EVACUATE_UPDATE_POINTERS_SLOTS)         \
  F(MINOR_MC_EVACUATE_UPDATE_POINTERS_TO_NEW_ROOTS)  \
  F(MINOR_MC_EVACUATE_UPDATE_POINTERS_WEAK)          \
  F(MINOR_MC_MARK)                                   \
  F(MINOR_MC_MARK_GLOBAL_HANDLES)                    \
  F(MINOR_MC_MARK_PARALLEL)                          \
  F(MINOR_MC_MARK_SEED)                              \
  F(MINOR_MC_MARK_ROOTS)                             \
  F(MINOR_MC_MARK_WEAK)                              \
  F(MINOR_MC_MARKING_DEQUE)                          \
  F(MINOR_MC_RESET_LIVENESS)                         \
  F(MINOR_MC_SWEEPING)                               \
  F(SCAVENGER_COMPLETE_SWEEP_ARRAY_BUFFERS)          \
  F(SCAVENGER_FAST_PROMOTE)                          \
  F(SCAVENGER_FREE_REMEMBERED_SET)                   \
  F(SCAVENGER_SCAVENGE)                              \
  F(SCAVENGER_PROCESS_ARRAY_BUFFERS)                 \
  F(SCAVENGER_SCAVENGE_WEAK_GLOBAL_HANDLES_IDENTIFY) \
  F(SCAVENGER_SCAVENGE_WEAK_GLOBAL_HANDLES_PROCESS)  \
  F(SCAVENGER_SCAVENGE_PARALLEL)                     \
  F(SCAVENGER_SCAVENGE_ROOTS)                        \
  F(SCAVENGER_SCAVENGE_UPDATE_REFS)                  \
  F(SCAVENGER_SCAVENGE_WEAK)                         \
  F(SCAVENGER_SCAVENGE_FINALIZE)                     \
  F(SCAVENGER_SWEEP_ARRAY_BUFFERS)

#define TRACER_BACKGROUND_SCOPES(F)               \
  F(BACKGROUND_ARRAY_BUFFER_FREE)                 \
  F(BACKGROUND_ARRAY_BUFFER_SWEEP)                \
  F(BACKGROUND_STORE_BUFFER)                      \
  F(BACKGROUND_UNMAPPER)                          \
  F(MC_BACKGROUND_EVACUATE_COPY)                  \
  F(MC_BACKGROUND_EVACUATE_UPDATE_POINTERS)       \
  F(MC_BACKGROUND_MARKING)                        \
  F(MC_BACKGROUND_SWEEPING)                       \
  F(MINOR_MC_BACKGROUND_EVACUATE_COPY)            \
  F(MINOR_MC_BACKGROUND_EVACUATE_UPDATE_POINTERS) \
  F(MINOR_MC_BACKGROUND_MARKING)                  \
  F(SCAVENGER_BACKGROUND_SCAVENGE_PARALLEL)

#endif  // V8_INIT_HEAP_SYMBOLS_H_
