// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_SYMBOLS_H_
#define V8_HEAP_SYMBOLS_H_

#define INTERNALIZED_STRING_LIST(V)                                \
  V(anonymous_string, "anonymous")                                 \
  V(apply_string, "apply")                                         \
  V(assign_string, "assign")                                       \
  V(arguments_string, "arguments")                                 \
  V(Arguments_string, "Arguments")                                 \
  V(Array_string, "Array")                                         \
  V(bind_string, "bind")                                           \
  V(bool16x8_string, "bool16x8")                                   \
  V(Bool16x8_string, "Bool16x8")                                   \
  V(bool32x4_string, "bool32x4")                                   \
  V(Bool32x4_string, "Bool32x4")                                   \
  V(bool8x16_string, "bool8x16")                                   \
  V(Bool8x16_string, "Bool8x16")                                   \
  V(boolean_string, "boolean")                                     \
  V(Boolean_string, "Boolean")                                     \
  V(bound__string, "bound ")                                       \
  V(byte_length_string, "byteLength")                              \
  V(byte_offset_string, "byteOffset")                              \
  V(call_string, "call")                                           \
  V(callee_string, "callee")                                       \
  V(caller_string, "caller")                                       \
  V(cell_value_string, "%cell_value")                              \
  V(char_at_string, "CharAt")                                      \
  V(closure_string, "(closure)")                                   \
  V(compare_ic_string, "==")                                       \
  V(configurable_string, "configurable")                           \
  V(constructor_string, "constructor")                             \
  V(construct_string, "construct")                                 \
  V(create_string, "create")                                       \
  V(Date_string, "Date")                                           \
  V(default_string, "default")                                     \
  V(defineProperty_string, "defineProperty")                       \
  V(deleteProperty_string, "deleteProperty")                       \
  V(display_name_string, "displayName")                            \
  V(done_string, "done")                                           \
  V(dot_result_string, ".result")                                  \
  V(dot_string, ".")                                               \
  V(entries_string, "entries")                                     \
  V(enumerable_string, "enumerable")                               \
  V(enumerate_string, "enumerate")                                 \
  V(Error_string, "Error")                                         \
  V(eval_string, "eval")                                           \
  V(false_string, "false")                                         \
  V(float32x4_string, "float32x4")                                 \
  V(Float32x4_string, "Float32x4")                                 \
  V(for_api_string, "for_api")                                     \
  V(for_string, "for")                                             \
  V(function_string, "function")                                   \
  V(Function_string, "Function")                                   \
  V(Generator_string, "Generator")                                 \
  V(getOwnPropertyDescriptor_string, "getOwnPropertyDescriptor")   \
  V(getOwnPropertyDescriptors_string, "getOwnPropertyDescriptors") \
  V(getPrototypeOf_string, "getPrototypeOf")                       \
  V(get_string, "get")                                             \
  V(global_string, "global")                                       \
  V(has_string, "has")                                             \
  V(illegal_access_string, "illegal access")                       \
  V(illegal_argument_string, "illegal argument")                   \
  V(index_string, "index")                                         \
  V(infinity_string, "Infinity")                                   \
  V(input_string, "input")                                         \
  V(int16x8_string, "int16x8")                                     \
  V(Int16x8_string, "Int16x8")                                     \
  V(int32x4_string, "int32x4")                                     \
  V(Int32x4_string, "Int32x4")                                     \
  V(int8x16_string, "int8x16")                                     \
  V(Int8x16_string, "Int8x16")                                     \
  V(isExtensible_string, "isExtensible")                           \
  V(isView_string, "isView")                                       \
  V(KeyedLoadMonomorphic_string, "KeyedLoadMonomorphic")           \
  V(KeyedStoreMonomorphic_string, "KeyedStoreMonomorphic")         \
  V(last_index_string, "lastIndex")                                \
  V(length_string, "length")                                       \
  V(Map_string, "Map")                                             \
  V(minus_infinity_string, "-Infinity")                            \
  V(minus_zero_string, "-0")                                       \
  V(name_string, "name")                                           \
  V(nan_string, "NaN")                                             \
  V(next_string, "next")                                           \
  V(null_string, "null")                                           \
  V(null_to_string, "[object Null]")                               \
  V(number_string, "number")                                       \
  V(Number_string, "Number")                                       \
  V(object_string, "object")                                       \
  V(Object_string, "Object")                                       \
  V(ownKeys_string, "ownKeys")                                     \
  V(preventExtensions_string, "preventExtensions")                 \
  V(private_api_string, "private_api")                             \
  V(Promise_string, "Promise")                                     \
  V(proto_string, "__proto__")                                     \
  V(prototype_string, "prototype")                                 \
  V(Proxy_string, "Proxy")                                         \
  V(query_colon_string, "(?:)")                                    \
  V(RegExp_string, "RegExp")                                       \
  V(setPrototypeOf_string, "setPrototypeOf")                       \
  V(set_string, "set")                                             \
  V(Set_string, "Set")                                             \
  V(source_mapping_url_string, "source_mapping_url")               \
  V(source_string, "source")                                       \
  V(source_url_string, "source_url")                               \
  V(stack_string, "stack")                                         \
  V(strict_compare_ic_string, "===")                               \
  V(string_string, "string")                                       \
  V(String_string, "String")                                       \
  V(symbol_string, "symbol")                                       \
  V(Symbol_string, "Symbol")                                       \
  V(this_string, "this")                                           \
  V(throw_string, "throw")                                         \
  V(toJSON_string, "toJSON")                                       \
  V(toString_string, "toString")                                   \
  V(true_string, "true")                                           \
  V(uint16x8_string, "uint16x8")                                   \
  V(Uint16x8_string, "Uint16x8")                                   \
  V(uint32x4_string, "uint32x4")                                   \
  V(Uint32x4_string, "Uint32x4")                                   \
  V(uint8x16_string, "uint8x16")                                   \
  V(Uint8x16_string, "Uint8x16")                                   \
  V(undefined_string, "undefined")                                 \
  V(undefined_to_string, "[object Undefined]")                     \
  V(valueOf_string, "valueOf")                                     \
  V(values_string, "values")                                       \
  V(value_string, "value")                                         \
  V(WeakMap_string, "WeakMap")                                     \
  V(WeakSet_string, "WeakSet")                                     \
  V(writable_string, "writable")

#define PRIVATE_SYMBOL_LIST(V)              \
  V(array_iteration_kind_symbol)            \
  V(array_iterator_next_symbol)             \
  V(array_iterator_object_symbol)           \
  V(call_site_function_symbol)              \
  V(call_site_position_symbol)              \
  V(call_site_receiver_symbol)              \
  V(call_site_strict_symbol)                \
  V(class_end_position_symbol)              \
  V(class_start_position_symbol)            \
  V(detailed_stack_trace_symbol)            \
  V(elements_transition_symbol)             \
  V(error_end_pos_symbol)                   \
  V(error_script_symbol)                    \
  V(error_start_pos_symbol)                 \
  V(formatted_stack_trace_symbol)           \
  V(frozen_symbol)                          \
  V(hash_code_symbol)                       \
  V(hidden_properties_symbol)               \
  V(home_object_symbol)                     \
  V(internal_error_symbol)                  \
  V(intl_impl_object_symbol)                \
  V(intl_initialized_marker_symbol)         \
  V(intl_pattern_symbol)                    \
  V(intl_resolved_symbol)                   \
  V(megamorphic_symbol)                     \
  V(native_context_index_symbol)            \
  V(nonexistent_symbol)                     \
  V(nonextensible_symbol)                   \
  V(normal_ic_symbol)                       \
  V(not_mapped_symbol)                      \
  V(observed_symbol)                        \
  V(premonomorphic_symbol)                  \
  V(promise_combined_deferred_symbol)       \
  V(promise_debug_marker_symbol)            \
  V(promise_has_handler_symbol)             \
  V(promise_on_resolve_symbol)              \
  V(promise_on_reject_symbol)               \
  V(promise_raw_symbol)                     \
  V(promise_status_symbol)                  \
  V(promise_value_symbol)                   \
  V(sealed_symbol)                          \
  V(stack_trace_symbol)                     \
  V(strict_function_transition_symbol)      \
  V(string_iterator_iterated_string_symbol) \
  V(string_iterator_next_index_symbol)      \
  V(strong_function_transition_symbol)      \
  V(uninitialized_symbol)

#define PUBLIC_SYMBOL_LIST(V)                \
  V(iterator_symbol, Symbol.iterator)        \
  V(match_symbol, Symbol.match)              \
  V(replace_symbol, Symbol.replace)          \
  V(search_symbol, Symbol.search)            \
  V(species_symbol, Symbol.species)          \
  V(split_symbol, Symbol.split)              \
  V(to_primitive_symbol, Symbol.toPrimitive) \
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
