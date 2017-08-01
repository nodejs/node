// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_SYMBOLS_H_
#define V8_HEAP_SYMBOLS_H_

#define INTERNALIZED_STRING_LIST(V)                                \
  V(anonymous_function_string, "(anonymous function)")             \
  V(anonymous_string, "anonymous")                                 \
  V(apply_string, "apply")                                         \
  V(arguments_string, "arguments")                                 \
  V(Arguments_string, "Arguments")                                 \
  V(arguments_to_string, "[object Arguments]")                     \
  V(Array_string, "Array")                                         \
  V(ArrayIterator_string, "Array Iterator")                        \
  V(assign_string, "assign")                                       \
  V(async_string, "async")                                         \
  V(await_string, "await")                                         \
  V(array_to_string, "[object Array]")                             \
  V(boolean_to_string, "[object Boolean]")                         \
  V(date_to_string, "[object Date]")                               \
  V(error_to_string, "[object Error]")                             \
  V(function_to_string, "[object Function]")                       \
  V(number_to_string, "[object Number]")                           \
  V(object_to_string, "[object Object]")                           \
  V(regexp_to_string, "[object RegExp]")                           \
  V(string_to_string, "[object String]")                           \
  V(bind_string, "bind")                                           \
  V(boolean_string, "boolean")                                     \
  V(Boolean_string, "Boolean")                                     \
  V(bound__string, "bound ")                                       \
  V(buffer_string, "buffer")                                       \
  V(byte_length_string, "byteLength")                              \
  V(byte_offset_string, "byteOffset")                              \
  V(call_string, "call")                                           \
  V(callee_string, "callee")                                       \
  V(caller_string, "caller")                                       \
  V(cell_value_string, "%cell_value")                              \
  V(char_at_string, "CharAt")                                      \
  V(closure_string, "(closure)")                                   \
  V(column_string, "column")                                       \
  V(compare_ic_string, "==")                                       \
  V(configurable_string, "configurable")                           \
  V(constructor_string, "constructor")                             \
  V(construct_string, "construct")                                 \
  V(create_string, "create")                                       \
  V(Date_string, "Date")                                           \
  V(dayperiod_string, "dayperiod")                                 \
  V(day_string, "day")                                             \
  V(default_string, "default")                                     \
  V(defineProperty_string, "defineProperty")                       \
  V(deleteProperty_string, "deleteProperty")                       \
  V(did_handle_string, "didHandle")                                \
  V(display_name_string, "displayName")                            \
  V(done_string, "done")                                           \
  V(dotAll_string, "dotAll")                                       \
  V(dot_catch_string, ".catch")                                    \
  V(dot_for_string, ".for")                                        \
  V(dot_generator_object_string, ".generator_object")              \
  V(dot_iterator_string, ".iterator")                              \
  V(dot_result_string, ".result")                                  \
  V(dot_switch_tag_string, ".switch_tag")                          \
  V(dot_string, ".")                                               \
  V(exec_string, "exec")                                           \
  V(entries_string, "entries")                                     \
  V(enqueue_string, "enqueue")                                     \
  V(enumerable_string, "enumerable")                               \
  V(era_string, "era")                                             \
  V(Error_string, "Error")                                         \
  V(eval_string, "eval")                                           \
  V(EvalError_string, "EvalError")                                 \
  V(false_string, "false")                                         \
  V(flags_string, "flags")                                         \
  V(function_string, "function")                                   \
  V(Function_string, "Function")                                   \
  V(Generator_string, "Generator")                                 \
  V(getOwnPropertyDescriptor_string, "getOwnPropertyDescriptor")   \
  V(getOwnPropertyDescriptors_string, "getOwnPropertyDescriptors") \
  V(getPrototypeOf_string, "getPrototypeOf")                       \
  V(get_string, "get")                                             \
  V(get_space_string, "get ")                                      \
  V(global_string, "global")                                       \
  V(groups_string, "groups")                                       \
  V(has_string, "has")                                             \
  V(hour_string, "hour")                                           \
  V(ignoreCase_string, "ignoreCase")                               \
  V(illegal_access_string, "illegal access")                       \
  V(illegal_argument_string, "illegal argument")                   \
  V(index_string, "index")                                         \
  V(infinity_string, "Infinity")                                   \
  V(input_string, "input")                                         \
  V(isExtensible_string, "isExtensible")                           \
  V(isView_string, "isView")                                       \
  V(KeyedLoadMonomorphic_string, "KeyedLoadMonomorphic")           \
  V(KeyedStoreMonomorphic_string, "KeyedStoreMonomorphic")         \
  V(keys_string, "keys")                                           \
  V(lastIndex_string, "lastIndex")                                 \
  V(length_string, "length")                                       \
  V(let_string, "let")                                             \
  V(line_string, "line")                                           \
  V(literal_string, "literal")                                     \
  V(Map_string, "Map")                                             \
  V(message_string, "message")                                     \
  V(minus_infinity_string, "-Infinity")                            \
  V(minus_zero_string, "-0")                                       \
  V(minute_string, "minute")                                       \
  V(Module_string, "Module")                                       \
  V(month_string, "month")                                         \
  V(multiline_string, "multiline")                                 \
  V(name_string, "name")                                           \
  V(native_string, "native")                                       \
  V(nan_string, "NaN")                                             \
  V(new_target_string, ".new.target")                              \
  V(next_string, "next")                                           \
  V(NFC_string, "NFC")                                             \
  V(NFD_string, "NFD")                                             \
  V(NFKC_string, "NFKC")                                           \
  V(NFKD_string, "NFKD")                                           \
  V(not_equal, "not-equal")                                        \
  V(null_string, "null")                                           \
  V(null_to_string, "[object Null]")                               \
  V(number_string, "number")                                       \
  V(Number_string, "Number")                                       \
  V(object_string, "object")                                       \
  V(Object_string, "Object")                                       \
  V(ok, "ok")                                                      \
  V(ownKeys_string, "ownKeys")                                     \
  V(position_string, "position")                                   \
  V(preventExtensions_string, "preventExtensions")                 \
  V(Promise_string, "Promise")                                     \
  V(PromiseResolveThenableJob_string, "PromiseResolveThenableJob") \
  V(promise_string, "promise")                                     \
  V(proto_string, "__proto__")                                     \
  V(prototype_string, "prototype")                                 \
  V(Proxy_string, "Proxy")                                         \
  V(query_colon_string, "(?:)")                                    \
  V(RangeError_string, "RangeError")                               \
  V(ReferenceError_string, "ReferenceError")                       \
  V(RegExp_string, "RegExp")                                       \
  V(reject_string, "reject")                                       \
  V(resolve_string, "resolve")                                     \
  V(return_string, "return")                                       \
  V(script_string, "script")                                       \
  V(second_string, "second")                                       \
  V(setPrototypeOf_string, "setPrototypeOf")                       \
  V(set_space_string, "set ")                                      \
  V(set_string, "set")                                             \
  V(Set_string, "Set")                                             \
  V(source_mapping_url_string, "source_mapping_url")               \
  V(source_string, "source")                                       \
  V(sourceText_string, "sourceText")                               \
  V(source_url_string, "source_url")                               \
  V(stack_string, "stack")                                         \
  V(stackTraceLimit_string, "stackTraceLimit")                     \
  V(star_default_star_string, "*default*")                         \
  V(sticky_string, "sticky")                                       \
  V(strict_compare_ic_string, "===")                               \
  V(string_string, "string")                                       \
  V(String_string, "String")                                       \
  V(symbol_string, "symbol")                                       \
  V(Symbol_string, "Symbol")                                       \
  V(symbol_species_string, "[Symbol.species]")                     \
  V(SyntaxError_string, "SyntaxError")                             \
  V(then_string, "then")                                           \
  V(this_function_string, ".this_function")                        \
  V(this_string, "this")                                           \
  V(throw_string, "throw")                                         \
  V(timed_out, "timed-out")                                        \
  V(timeZoneName_string, "timeZoneName")                           \
  V(toJSON_string, "toJSON")                                       \
  V(toString_string, "toString")                                   \
  V(true_string, "true")                                           \
  V(TypeError_string, "TypeError")                                 \
  V(type_string, "type")                                           \
  V(CompileError_string, "CompileError")                           \
  V(LinkError_string, "LinkError")                                 \
  V(RuntimeError_string, "RuntimeError")                           \
  V(undefined_string, "undefined")                                 \
  V(undefined_to_string, "[object Undefined]")                     \
  V(unicode_string, "unicode")                                     \
  V(use_asm_string, "use asm")                                     \
  V(use_strict_string, "use strict")                               \
  V(URIError_string, "URIError")                                   \
  V(valueOf_string, "valueOf")                                     \
  V(values_string, "values")                                       \
  V(value_string, "value")                                         \
  V(WeakMap_string, "WeakMap")                                     \
  V(WeakSet_string, "WeakSet")                                     \
  V(weekday_string, "weekday")                                     \
  V(will_handle_string, "willHandle")                              \
  V(writable_string, "writable")                                   \
  V(year_string, "year")

#define PRIVATE_SYMBOL_LIST(V)              \
  V(array_iteration_kind_symbol)            \
  V(array_iterator_next_symbol)             \
  V(array_iterator_object_symbol)           \
  V(call_site_frame_array_symbol)           \
  V(call_site_frame_index_symbol)           \
  V(class_end_position_symbol)              \
  V(class_start_position_symbol)            \
  V(detailed_stack_trace_symbol)            \
  V(elements_transition_symbol)             \
  V(error_end_pos_symbol)                   \
  V(error_script_symbol)                    \
  V(error_start_pos_symbol)                 \
  V(frozen_symbol)                          \
  V(hash_code_symbol)                       \
  V(home_object_symbol)                     \
  V(intl_initialized_marker_symbol)         \
  V(intl_pattern_symbol)                    \
  V(intl_resolved_symbol)                   \
  V(megamorphic_symbol)                     \
  V(native_context_index_symbol)            \
  V(nonextensible_symbol)                   \
  V(not_mapped_symbol)                      \
  V(premonomorphic_symbol)                  \
  V(promise_async_stack_id_symbol)          \
  V(promise_debug_marker_symbol)            \
  V(promise_forwarding_handler_symbol)      \
  V(promise_handled_by_symbol)              \
  V(promise_async_id_symbol)                \
  V(promise_default_resolve_handler_symbol) \
  V(promise_default_reject_handler_symbol)  \
  V(sealed_symbol)                          \
  V(stack_trace_symbol)                     \
  V(strict_function_transition_symbol)      \
  V(uninitialized_symbol)

#define PUBLIC_SYMBOL_LIST(V)                    \
  V(async_iterator_symbol, Symbol.asyncIterator) \
  V(iterator_symbol, Symbol.iterator)            \
  V(intl_fallback_symbol, IntlFallback)          \
  V(match_symbol, Symbol.match)                  \
  V(replace_symbol, Symbol.replace)              \
  V(search_symbol, Symbol.search)                \
  V(species_symbol, Symbol.species)              \
  V(split_symbol, Symbol.split)                  \
  V(to_primitive_symbol, Symbol.toPrimitive)     \
  V(unscopables_symbol, Symbol.unscopables)

// Well-Known Symbols are "Public" symbols, which have a bit set which causes
// them to produce an undefined value when a load results in a failed access
// check. Because this behaviour is not specified properly as of yet, it only
// applies to a subset of spec-defined Well-Known Symbols.
#define WELL_KNOWN_SYMBOL_LIST(V)                           \
  V(has_instance_symbol, Symbol.hasInstance)                \
  V(is_concat_spreadable_symbol, Symbol.isConcatSpreadable) \
  V(to_string_tag_symbol, Symbol.toStringTag)

#endif  // V8_HEAP_SYMBOLS_H_
