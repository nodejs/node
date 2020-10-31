## Process this file with automake to produce Makefile.in

if HAVE_ZLIB
GZCHECKPROGRAMS = zcgzip zcgunzip
GZHEADERS = google/protobuf/io/gzip_stream.h
GZTESTS = google/protobuf/io/gzip_stream_unittest.sh
ZLIB_DEF = -DHAVE_ZLIB=1
else
GZCHECKPROGRAMS =
GZHEADERS =
GZTESTS =
ZLIB_DEF =
endif

if HAVE_PTHREAD
PTHREAD_DEF = -DHAVE_PTHREAD=1
else
PTHREAD_DEF =
endif

PROTOBUF_VERSION = 20:0:0

if GCC
# Turn on all warnings except for sign comparison (we ignore sign comparison
# in Google so our code base have tons of such warnings).
NO_OPT_CXXFLAGS = $(PTHREAD_CFLAGS) $(PTHREAD_DEF) $(ZLIB_DEF) -Wall -Wno-sign-compare
else
NO_OPT_CXXFLAGS = $(PTHREAD_CFLAGS) $(PTHREAD_DEF) $(ZLIB_DEF)
endif

AM_CXXFLAGS = $(NO_OPT_CXXFLAGS) $(PROTOBUF_OPT_FLAG)

AM_LDFLAGS = $(PTHREAD_CFLAGS)

# If I say "dist_include_DATA", automake complains that $(includedir) is not
# a "legitimate" directory for DATA.  Screw you, automake.
protodir = $(includedir)

# If you are adding new files here, also remember to change the build files for
# all other languages, //protoc-artifacts/build-zip.sh and run
# //update_file_list.sh for bazel.
nobase_dist_proto_DATA = google/protobuf/descriptor.proto      \
                         google/protobuf/any.proto             \
                         google/protobuf/api.proto             \
                         google/protobuf/duration.proto        \
                         google/protobuf/empty.proto           \
                         google/protobuf/field_mask.proto      \
                         google/protobuf/source_context.proto  \
                         google/protobuf/struct.proto          \
                         google/protobuf/timestamp.proto       \
                         google/protobuf/type.proto            \
                         google/protobuf/wrappers.proto        \
                         google/protobuf/compiler/plugin.proto

# Not sure why these don't get cleaned automatically.
clean-local:
	rm -f *.loT

CLEANFILES = $(protoc_outputs) unittest_proto_middleman \
             testzip.jar testzip.list testzip.proto testzip.zip \
             no_warning_test.cc

MAINTAINERCLEANFILES =   \
  Makefile.in

nobase_include_HEADERS =                                         \
  google/protobuf/stubs/callback.h                               \
  google/protobuf/stubs/bytestream.h                             \
  google/protobuf/stubs/casts.h                                  \
  google/protobuf/stubs/common.h                                 \
  google/protobuf/stubs/fastmem.h                                \
  google/protobuf/stubs/hash.h                                   \
  google/protobuf/stubs/logging.h                                \
  google/protobuf/stubs/macros.h                                 \
  google/protobuf/stubs/map_util.h                               \
  google/protobuf/stubs/mutex.h                                  \
  google/protobuf/stubs/once.h                                   \
  google/protobuf/stubs/platform_macros.h                        \
  google/protobuf/stubs/port.h                                   \
  google/protobuf/stubs/status.h                                 \
  google/protobuf/stubs/stl_util.h                               \
  google/protobuf/stubs/stringpiece.h                            \
  google/protobuf/stubs/strutil.h                                \
  google/protobuf/stubs/template_util.h                          \
  google/protobuf/any.pb.h                                       \
  google/protobuf/api.pb.h                                       \
  google/protobuf/any.h                                          \
  google/protobuf/arena.h                                        \
  google/protobuf/arena_impl.h                                   \
  google/protobuf/arenastring.h                                  \
  google/protobuf/descriptor_database.h                          \
  google/protobuf/descriptor.h                                   \
  google/protobuf/descriptor.pb.h                                \
  google/protobuf/duration.pb.h                                  \
  google/protobuf/dynamic_message.h                              \
  google/protobuf/empty.pb.h                                     \
  google/protobuf/extension_set.h                                \
  google/protobuf/extension_set_inl.h                            \
  google/protobuf/field_mask.pb.h                                \
  google/protobuf/generated_enum_reflection.h                    \
  google/protobuf/generated_enum_util.h                          \
  google/protobuf/generated_message_reflection.h                 \
  google/protobuf/generated_message_table_driven.h               \
  google/protobuf/generated_message_util.h                       \
  google/protobuf/has_bits.h                                     \
  google/protobuf/implicit_weak_message.h                        \
  google/protobuf/inlined_string_field.h                         \
  google/protobuf/map_entry.h                                    \
  google/protobuf/map_entry_lite.h                               \
  google/protobuf/map_field.h                                    \
  google/protobuf/map_field_inl.h                                \
  google/protobuf/map_field_lite.h                               \
  google/protobuf/map.h                                          \
  google/protobuf/map_type_handler.h                             \
  google/protobuf/message.h                                      \
  google/protobuf/message_lite.h                                 \
  google/protobuf/metadata.h                                     \
  google/protobuf/metadata_lite.h                                \
  google/protobuf/parse_context.h                                \
  google/protobuf/port.h                                         \
  google/protobuf/port_def.inc                                   \
  google/protobuf/port_undef.inc                                 \
  google/protobuf/reflection.h                                   \
  google/protobuf/reflection_ops.h                               \
  google/protobuf/repeated_field.h                               \
  google/protobuf/service.h                                      \
  google/protobuf/source_context.pb.h                            \
  google/protobuf/struct.pb.h                                    \
  google/protobuf/text_format.h                                  \
  google/protobuf/timestamp.pb.h                                 \
  google/protobuf/type.pb.h                                      \
  google/protobuf/unknown_field_set.h                            \
  google/protobuf/wire_format.h                                  \
  google/protobuf/wire_format_lite.h                             \
  google/protobuf/wrappers.pb.h                                  \
  google/protobuf/io/coded_stream.h                              \
  $(GZHEADERS)                                                   \
  google/protobuf/io/printer.h                                   \
  google/protobuf/io/strtod.h                                    \
  google/protobuf/io/tokenizer.h                                 \
  google/protobuf/io/zero_copy_stream.h                          \
  google/protobuf/io/zero_copy_stream_impl.h                     \
  google/protobuf/io/zero_copy_stream_impl_lite.h                \
  google/protobuf/compiler/code_generator.h                      \
  google/protobuf/compiler/command_line_interface.h              \
  google/protobuf/compiler/importer.h                            \
  google/protobuf/compiler/parser.h                              \
  google/protobuf/compiler/plugin.h                              \
  google/protobuf/compiler/plugin.pb.h                           \
  google/protobuf/compiler/cpp/cpp_generator.h                   \
  google/protobuf/compiler/csharp/csharp_generator.h             \
  google/protobuf/compiler/csharp/csharp_names.h                 \
  google/protobuf/compiler/java/java_generator.h                 \
  google/protobuf/compiler/java/java_names.h                     \
  google/protobuf/compiler/js/js_generator.h                     \
  google/protobuf/compiler/js/well_known_types_embed.h           \
  google/protobuf/compiler/objectivec/objectivec_generator.h     \
  google/protobuf/compiler/objectivec/objectivec_helpers.h       \
  google/protobuf/compiler/php/php_generator.h                   \
  google/protobuf/compiler/python/python_generator.h             \
  google/protobuf/compiler/ruby/ruby_generator.h                 \
  google/protobuf/util/type_resolver.h                           \
  google/protobuf/util/delimited_message_util.h                  \
  google/protobuf/util/field_comparator.h                        \
  google/protobuf/util/field_mask_util.h                         \
  google/protobuf/util/json_util.h                               \
  google/protobuf/util/time_util.h                               \
  google/protobuf/util/type_resolver_util.h                      \
  google/protobuf/util/message_differencer.h

lib_LTLIBRARIES = libprotobuf-lite.la libprotobuf.la libprotoc.la

libprotobuf_lite_la_LIBADD = $(PTHREAD_LIBS) $(LIBATOMIC_LIBS)
libprotobuf_lite_la_LDFLAGS = -version-info $(PROTOBUF_VERSION) -export-dynamic -no-undefined
if HAVE_LD_VERSION_SCRIPT
libprotobuf_lite_la_LDFLAGS += -Wl,--version-script=$(srcdir)/libprotobuf-lite.map
EXTRA_libprotobuf_lite_la_DEPENDENCIES = libprotobuf-lite.map
endif
libprotobuf_lite_la_SOURCES =                                  \
  google/protobuf/stubs/bytestream.cc                          \
  google/protobuf/stubs/bytestream.h                           \
  google/protobuf/stubs/common.cc                              \
  google/protobuf/stubs/hash.h                                 \
  google/protobuf/stubs/int128.cc                              \
  google/protobuf/stubs/int128.h                               \
  google/protobuf/io/io_win32.cc                               \
  google/protobuf/io/io_win32.h                                \
  google/protobuf/stubs/map_util.h                             \
  google/protobuf/stubs/mathutil.h                             \
  google/protobuf/stubs/status.cc                              \
  google/protobuf/stubs/status.h                               \
  google/protobuf/stubs/status_macros.h                        \
  google/protobuf/stubs/statusor.cc                            \
  google/protobuf/stubs/statusor.h                             \
  google/protobuf/stubs/stringpiece.cc                         \
  google/protobuf/stubs/stringpiece.h                          \
  google/protobuf/stubs/stringprintf.cc                        \
  google/protobuf/stubs/stringprintf.h                         \
  google/protobuf/stubs/structurally_valid.cc                  \
  google/protobuf/stubs/strutil.cc                             \
  google/protobuf/stubs/time.cc                                \
  google/protobuf/stubs/time.h                                 \
  google/protobuf/any_lite.cc                                  \
  google/protobuf/arena.cc                                     \
  google/protobuf/extension_set.cc                             \
  google/protobuf/generated_enum_util.cc                       \
  google/protobuf/generated_message_util.cc                    \
  google/protobuf/generated_message_table_driven_lite.h        \
  google/protobuf/generated_message_table_driven_lite.cc       \
  google/protobuf/implicit_weak_message.cc                     \
  google/protobuf/message_lite.cc                              \
  google/protobuf/parse_context.cc                             \
  google/protobuf/repeated_field.cc                            \
  google/protobuf/wire_format_lite.cc                          \
  google/protobuf/io/coded_stream.cc                           \
  google/protobuf/io/coded_stream_inl.h                        \
  google/protobuf/io/strtod.cc                                 \
  google/protobuf/io/zero_copy_stream.cc                       \
  google/protobuf/io/zero_copy_stream_impl.cc                  \
  google/protobuf/io/zero_copy_stream_impl_lite.cc

libprotobuf_la_LIBADD = $(PTHREAD_LIBS) $(LIBATOMIC_LIBS)
libprotobuf_la_LDFLAGS = -version-info $(PROTOBUF_VERSION) -export-dynamic -no-undefined
if HAVE_LD_VERSION_SCRIPT
libprotobuf_la_LDFLAGS += -Wl,--version-script=$(srcdir)/libprotobuf.map
EXTRA_libprotobuf_la_DEPENDENCIES = libprotobuf.map
endif
libprotobuf_la_SOURCES =                                       \
  $(libprotobuf_lite_la_SOURCES)                               \
  google/protobuf/any.pb.cc                                    \
  google/protobuf/api.pb.cc                                    \
  google/protobuf/stubs/mathlimits.cc                          \
  google/protobuf/stubs/mathlimits.h                           \
  google/protobuf/any.cc                                       \
  google/protobuf/descriptor.cc                                \
  google/protobuf/descriptor_database.cc                       \
  google/protobuf/descriptor.pb.cc                             \
  google/protobuf/duration.pb.cc                               \
  google/protobuf/dynamic_message.cc                           \
  google/protobuf/empty.pb.cc                                  \
  google/protobuf/extension_set_heavy.cc                       \
  google/protobuf/field_mask.pb.cc                             \
  google/protobuf/generated_message_reflection.cc              \
  google/protobuf/generated_message_table_driven_lite.h        \
  google/protobuf/generated_message_table_driven.cc            \
  google/protobuf/map_field.cc                                 \
  google/protobuf/message.cc                                   \
  google/protobuf/reflection_internal.h                        \
  google/protobuf/reflection_ops.cc                            \
  google/protobuf/service.cc                                   \
  google/protobuf/source_context.pb.cc                         \
  google/protobuf/struct.pb.cc                                 \
  google/protobuf/stubs/substitute.cc                          \
  google/protobuf/stubs/substitute.h                           \
  google/protobuf/text_format.cc                               \
  google/protobuf/timestamp.pb.cc                              \
  google/protobuf/type.pb.cc                                   \
  google/protobuf/unknown_field_set.cc                         \
  google/protobuf/wire_format.cc                               \
  google/protobuf/wrappers.pb.cc                               \
  google/protobuf/io/gzip_stream.cc                            \
  google/protobuf/io/printer.cc                                \
  google/protobuf/io/tokenizer.cc                              \
  google/protobuf/compiler/importer.cc                         \
  google/protobuf/compiler/parser.cc                           \
  google/protobuf/util/delimited_message_util.cc               \
  google/protobuf/util/field_comparator.cc                     \
  google/protobuf/util/field_mask_util.cc                      \
  google/protobuf/util/internal/constants.h                    \
  google/protobuf/util/internal/datapiece.cc                   \
  google/protobuf/util/internal/datapiece.h                    \
  google/protobuf/util/internal/default_value_objectwriter.cc  \
  google/protobuf/util/internal/default_value_objectwriter.h   \
  google/protobuf/util/internal/error_listener.cc              \
  google/protobuf/util/internal/error_listener.h               \
  google/protobuf/util/internal/expecting_objectwriter.h       \
  google/protobuf/util/internal/field_mask_utility.cc          \
  google/protobuf/util/internal/field_mask_utility.h           \
  google/protobuf/util/internal/json_escaping.cc               \
  google/protobuf/util/internal/json_escaping.h                \
  google/protobuf/util/internal/json_objectwriter.cc           \
  google/protobuf/util/internal/json_objectwriter.h            \
  google/protobuf/util/internal/json_stream_parser.cc          \
  google/protobuf/util/internal/json_stream_parser.h           \
  google/protobuf/util/internal/location_tracker.h             \
  google/protobuf/util/internal/mock_error_listener.h          \
  google/protobuf/util/internal/object_location_tracker.h      \
  google/protobuf/util/internal/object_source.h                \
  google/protobuf/util/internal/object_writer.cc               \
  google/protobuf/util/internal/object_writer.h                \
  google/protobuf/util/internal/protostream_objectsource.cc    \
  google/protobuf/util/internal/protostream_objectsource.h     \
  google/protobuf/util/internal/protostream_objectwriter.cc    \
  google/protobuf/util/internal/protostream_objectwriter.h     \
  google/protobuf/util/internal/proto_writer.cc                \
  google/protobuf/util/internal/proto_writer.h                 \
  google/protobuf/util/internal/structured_objectwriter.h      \
  google/protobuf/util/internal/type_info.cc                   \
  google/protobuf/util/internal/type_info.h                    \
  google/protobuf/util/internal/type_info_test_helper.cc       \
  google/protobuf/util/internal/type_info_test_helper.h        \
  google/protobuf/util/internal/utility.cc                     \
  google/protobuf/util/internal/utility.h                      \
  google/protobuf/util/json_util.cc                            \
  google/protobuf/util/message_differencer.cc                  \
  google/protobuf/util/time_util.cc                            \
  google/protobuf/util/type_resolver_util.cc

nodist_libprotobuf_la_SOURCES = $(nodist_libprotobuf_lite_la_SOURCES)

libprotoc_la_LIBADD = $(PTHREAD_LIBS) libprotobuf.la
libprotoc_la_LDFLAGS = -version-info $(PROTOBUF_VERSION) -export-dynamic -no-undefined
if HAVE_LD_VERSION_SCRIPT
libprotoc_la_LDFLAGS += -Wl,--version-script=$(srcdir)/libprotoc.map
EXTRA_libprotoc_la_DEPENDENCIES = libprotoc.map
endif
libprotoc_la_SOURCES =                                         \
  google/protobuf/compiler/code_generator.cc                   \
  google/protobuf/compiler/command_line_interface.cc           \
  google/protobuf/compiler/plugin.cc                           \
  google/protobuf/compiler/plugin.pb.cc                        \
  google/protobuf/compiler/scc.h                               \
  google/protobuf/compiler/subprocess.cc                       \
  google/protobuf/compiler/subprocess.h                        \
  google/protobuf/compiler/zip_writer.cc                       \
  google/protobuf/compiler/zip_writer.h                        \
  google/protobuf/compiler/cpp/cpp_enum.cc                     \
  google/protobuf/compiler/cpp/cpp_enum.h                      \
  google/protobuf/compiler/cpp/cpp_enum_field.cc               \
  google/protobuf/compiler/cpp/cpp_enum_field.h                \
  google/protobuf/compiler/cpp/cpp_extension.cc                \
  google/protobuf/compiler/cpp/cpp_extension.h                 \
  google/protobuf/compiler/cpp/cpp_field.cc                    \
  google/protobuf/compiler/cpp/cpp_field.h                     \
  google/protobuf/compiler/cpp/cpp_file.cc                     \
  google/protobuf/compiler/cpp/cpp_file.h                      \
  google/protobuf/compiler/cpp/cpp_generator.cc                \
  google/protobuf/compiler/cpp/cpp_helpers.cc                  \
  google/protobuf/compiler/cpp/cpp_helpers.h                   \
  google/protobuf/compiler/cpp/cpp_map_field.cc                \
  google/protobuf/compiler/cpp/cpp_map_field.h                 \
  google/protobuf/compiler/cpp/cpp_message.cc                  \
  google/protobuf/compiler/cpp/cpp_message.h                   \
  google/protobuf/compiler/cpp/cpp_message_field.cc            \
  google/protobuf/compiler/cpp/cpp_message_field.h             \
  google/protobuf/compiler/cpp/cpp_message_layout_helper.h     \
  google/protobuf/compiler/cpp/cpp_options.h                   \
  google/protobuf/compiler/cpp/cpp_padding_optimizer.cc        \
  google/protobuf/compiler/cpp/cpp_padding_optimizer.h         \
  google/protobuf/compiler/cpp/cpp_primitive_field.cc          \
  google/protobuf/compiler/cpp/cpp_primitive_field.h           \
  google/protobuf/compiler/cpp/cpp_service.cc                  \
  google/protobuf/compiler/cpp/cpp_service.h                   \
  google/protobuf/compiler/cpp/cpp_string_field.cc             \
  google/protobuf/compiler/cpp/cpp_string_field.h              \
  google/protobuf/compiler/java/java_context.cc                \
  google/protobuf/compiler/java/java_context.h                 \
  google/protobuf/compiler/java/java_enum.cc                   \
  google/protobuf/compiler/java/java_enum_lite.cc              \
  google/protobuf/compiler/java/java_enum_field.cc             \
  google/protobuf/compiler/java/java_enum_field.h              \
  google/protobuf/compiler/java/java_enum_field_lite.cc        \
  google/protobuf/compiler/java/java_enum_field_lite.h         \
  google/protobuf/compiler/java/java_enum.h                    \
  google/protobuf/compiler/java/java_enum_lite.h               \
  google/protobuf/compiler/java/java_extension.cc              \
  google/protobuf/compiler/java/java_extension.h               \
  google/protobuf/compiler/java/java_extension_lite.cc         \
  google/protobuf/compiler/java/java_extension_lite.h          \
  google/protobuf/compiler/java/java_field.cc                  \
  google/protobuf/compiler/java/java_field.h                   \
  google/protobuf/compiler/java/java_file.cc                   \
  google/protobuf/compiler/java/java_file.h                    \
  google/protobuf/compiler/java/java_generator.cc              \
  google/protobuf/compiler/java/java_generator_factory.cc      \
  google/protobuf/compiler/java/java_generator_factory.h       \
  google/protobuf/compiler/java/java_helpers.cc                \
  google/protobuf/compiler/java/java_helpers.h                 \
  google/protobuf/compiler/java/java_map_field.cc              \
  google/protobuf/compiler/java/java_map_field.h               \
  google/protobuf/compiler/java/java_map_field_lite.cc         \
  google/protobuf/compiler/java/java_map_field_lite.h          \
  google/protobuf/compiler/java/java_message.cc                \
  google/protobuf/compiler/java/java_message_lite.cc           \
  google/protobuf/compiler/java/java_message_builder.cc        \
  google/protobuf/compiler/java/java_message_builder_lite.cc   \
  google/protobuf/compiler/java/java_message_field.cc          \
  google/protobuf/compiler/java/java_message_field.h           \
  google/protobuf/compiler/java/java_message_field_lite.cc     \
  google/protobuf/compiler/java/java_message_field_lite.h      \
  google/protobuf/compiler/java/java_message.h                 \
  google/protobuf/compiler/java/java_message_lite.h            \
  google/protobuf/compiler/java/java_message_builder.h         \
  google/protobuf/compiler/java/java_message_builder_lite.h    \
  google/protobuf/compiler/java/java_name_resolver.cc          \
  google/protobuf/compiler/java/java_name_resolver.h           \
  google/protobuf/compiler/java/java_options.h                 \
  google/protobuf/compiler/java/java_primitive_field.cc        \
  google/protobuf/compiler/java/java_primitive_field.h         \
  google/protobuf/compiler/java/java_primitive_field_lite.cc   \
  google/protobuf/compiler/java/java_primitive_field_lite.h    \
  google/protobuf/compiler/java/java_shared_code_generator.cc  \
  google/protobuf/compiler/java/java_shared_code_generator.h   \
  google/protobuf/compiler/java/java_service.cc                \
  google/protobuf/compiler/java/java_service.h                 \
  google/protobuf/compiler/java/java_string_field.cc           \
  google/protobuf/compiler/java/java_string_field.h            \
  google/protobuf/compiler/java/java_string_field_lite.cc      \
  google/protobuf/compiler/java/java_string_field_lite.h       \
  google/protobuf/compiler/java/java_doc_comment.cc            \
  google/protobuf/compiler/java/java_doc_comment.h             \
  google/protobuf/compiler/js/js_generator.cc                  \
  google/protobuf/compiler/js/well_known_types_embed.cc        \
  google/protobuf/compiler/objectivec/objectivec_enum.cc       \
  google/protobuf/compiler/objectivec/objectivec_enum.h        \
  google/protobuf/compiler/objectivec/objectivec_enum_field.cc \
  google/protobuf/compiler/objectivec/objectivec_enum_field.h  \
  google/protobuf/compiler/objectivec/objectivec_extension.cc  \
  google/protobuf/compiler/objectivec/objectivec_extension.h   \
  google/protobuf/compiler/objectivec/objectivec_field.cc      \
  google/protobuf/compiler/objectivec/objectivec_field.h       \
  google/protobuf/compiler/objectivec/objectivec_file.cc       \
  google/protobuf/compiler/objectivec/objectivec_file.h        \
  google/protobuf/compiler/objectivec/objectivec_generator.cc  \
  google/protobuf/compiler/objectivec/objectivec_helpers.cc    \
  google/protobuf/compiler/objectivec/objectivec_helpers.h     \
  google/protobuf/compiler/objectivec/objectivec_map_field.cc  \
  google/protobuf/compiler/objectivec/objectivec_map_field.h   \
  google/protobuf/compiler/objectivec/objectivec_message.cc    \
  google/protobuf/compiler/objectivec/objectivec_message.h     \
  google/protobuf/compiler/objectivec/objectivec_message_field.cc \
  google/protobuf/compiler/objectivec/objectivec_message_field.h \
  google/protobuf/compiler/objectivec/objectivec_nsobject_methods.h  \
  google/protobuf/compiler/objectivec/objectivec_oneof.cc      \
  google/protobuf/compiler/objectivec/objectivec_oneof.h       \
  google/protobuf/compiler/objectivec/objectivec_primitive_field.cc \
  google/protobuf/compiler/objectivec/objectivec_primitive_field.h \
  google/protobuf/compiler/php/php_generator.cc                \
  google/protobuf/compiler/python/python_generator.cc          \
  google/protobuf/compiler/ruby/ruby_generator.cc              \
  google/protobuf/compiler/csharp/csharp_doc_comment.cc        \
  google/protobuf/compiler/csharp/csharp_doc_comment.h         \
  google/protobuf/compiler/csharp/csharp_enum.cc               \
  google/protobuf/compiler/csharp/csharp_enum.h                \
  google/protobuf/compiler/csharp/csharp_enum_field.cc         \
  google/protobuf/compiler/csharp/csharp_enum_field.h          \
  google/protobuf/compiler/csharp/csharp_field_base.cc         \
  google/protobuf/compiler/csharp/csharp_field_base.h          \
  google/protobuf/compiler/csharp/csharp_generator.cc          \
  google/protobuf/compiler/csharp/csharp_helpers.cc            \
  google/protobuf/compiler/csharp/csharp_helpers.h             \
  google/protobuf/compiler/csharp/csharp_map_field.cc          \
  google/protobuf/compiler/csharp/csharp_map_field.h           \
  google/protobuf/compiler/csharp/csharp_message.cc            \
  google/protobuf/compiler/csharp/csharp_message.h             \
  google/protobuf/compiler/csharp/csharp_message_field.cc      \
  google/protobuf/compiler/csharp/csharp_message_field.h       \
  google/protobuf/compiler/csharp/csharp_options.h             \
  google/protobuf/compiler/csharp/csharp_primitive_field.cc    \
  google/protobuf/compiler/csharp/csharp_primitive_field.h     \
  google/protobuf/compiler/csharp/csharp_reflection_class.cc     \
  google/protobuf/compiler/csharp/csharp_reflection_class.h      \
  google/protobuf/compiler/csharp/csharp_repeated_enum_field.cc \
  google/protobuf/compiler/csharp/csharp_repeated_enum_field.h \
  google/protobuf/compiler/csharp/csharp_repeated_message_field.cc \
  google/protobuf/compiler/csharp/csharp_repeated_message_field.h \
  google/protobuf/compiler/csharp/csharp_repeated_primitive_field.cc \
  google/protobuf/compiler/csharp/csharp_repeated_primitive_field.h \
  google/protobuf/compiler/csharp/csharp_source_generator_base.cc \
  google/protobuf/compiler/csharp/csharp_source_generator_base.h \
  google/protobuf/compiler/csharp/csharp_wrapper_field.cc      \
  google/protobuf/compiler/csharp/csharp_wrapper_field.h

bin_PROGRAMS = protoc
protoc_LDADD = $(PTHREAD_LIBS) libprotobuf.la libprotoc.la
protoc_SOURCES = google/protobuf/compiler/main.cc

# Tests ==============================================================

protoc_inputs =                                                   \
  google/protobuf/any_test.proto                                  \
  google/protobuf/compiler/cpp/cpp_test_bad_identifiers.proto     \
  google/protobuf/map_lite_unittest.proto                         \
  google/protobuf/map_proto2_unittest.proto                       \
  google/protobuf/map_unittest.proto                              \
  google/protobuf/unittest_arena.proto                            \
  google/protobuf/unittest_custom_options.proto                   \
  google/protobuf/unittest_drop_unknown_fields.proto              \
  google/protobuf/unittest_embed_optimize_for.proto               \
  google/protobuf/unittest_empty.proto                            \
  google/protobuf/unittest_enormous_descriptor.proto              \
  google/protobuf/unittest_import_lite.proto                      \
  google/protobuf/unittest_import.proto                           \
  google/protobuf/unittest_import_public_lite.proto               \
  google/protobuf/unittest_import_public.proto                    \
  google/protobuf/unittest_lazy_dependencies.proto                \
  google/protobuf/unittest_lazy_dependencies_custom_option.proto  \
  google/protobuf/unittest_lazy_dependencies_enum.proto           \
  google/protobuf/unittest_lite_imports_nonlite.proto             \
  google/protobuf/unittest_lite.proto                             \
  google/protobuf/unittest_mset.proto                             \
  google/protobuf/unittest_mset_wire_format.proto                 \
  google/protobuf/unittest_no_arena_lite.proto                    \
  google/protobuf/unittest_no_arena_import.proto                  \
  google/protobuf/unittest_no_arena.proto                         \
  google/protobuf/unittest_no_field_presence.proto                \
  google/protobuf/unittest_no_generic_services.proto              \
  google/protobuf/unittest_optimize_for.proto                     \
  google/protobuf/unittest_preserve_unknown_enum2.proto           \
  google/protobuf/unittest_preserve_unknown_enum.proto            \
  google/protobuf/unittest.proto                                  \
  google/protobuf/unittest_proto3.proto                           \
  google/protobuf/unittest_proto3_arena.proto                     \
  google/protobuf/unittest_proto3_arena_lite.proto                \
  google/protobuf/unittest_proto3_lite.proto                      \
  google/protobuf/unittest_well_known_types.proto                 \
  google/protobuf/util/internal/testdata/anys.proto               \
  google/protobuf/util/internal/testdata/books.proto              \
  google/protobuf/util/internal/testdata/default_value.proto      \
  google/protobuf/util/internal/testdata/default_value_test.proto \
  google/protobuf/util/internal/testdata/field_mask.proto         \
  google/protobuf/util/internal/testdata/maps.proto               \
  google/protobuf/util/internal/testdata/oneofs.proto             \
  google/protobuf/util/internal/testdata/proto3.proto             \
  google/protobuf/util/internal/testdata/struct.proto             \
  google/protobuf/util/internal/testdata/timestamp_duration.proto \
  google/protobuf/util/internal/testdata/wrappers.proto           \
  google/protobuf/util/json_format.proto                          \
  google/protobuf/util/json_format_proto3.proto                   \
  google/protobuf/util/message_differencer_unittest.proto         \
  google/protobuf/compiler/cpp/cpp_test_large_enum_value.proto

EXTRA_DIST =                                                   \
  $(protoc_inputs)                                             \
  solaris/libstdc++.la                                         \
  google/protobuf/test_messages_proto3.proto                   \
  google/protobuf/test_messages_proto2.proto                   \
  google/protobuf/io/gzip_stream.h                             \
  google/protobuf/io/gzip_stream_unittest.sh                   \
  google/protobuf/testdata/golden_message                      \
  google/protobuf/testdata/golden_message_maps                 \
  google/protobuf/testdata/golden_message_oneof_implemented    \
  google/protobuf/testdata/golden_message_proto3               \
  google/protobuf/testdata/golden_packed_fields_message        \
  google/protobuf/testdata/bad_utf8_string                     \
  google/protobuf/testdata/map_test_data.txt                   \
  google/protobuf/testdata/text_format_unittest_data.txt       \
  google/protobuf/testdata/text_format_unittest_data_oneof_implemented.txt  \
  google/protobuf/testdata/text_format_unittest_data_pointy.txt             \
  google/protobuf/testdata/text_format_unittest_data_pointy_oneof.txt       \
  google/protobuf/testdata/text_format_unittest_extensions_data.txt         \
  google/protobuf/testdata/text_format_unittest_extensions_data_pointy.txt  \
  google/protobuf/package_info.h                               \
  google/protobuf/io/package_info.h                            \
  google/protobuf/util/package_info.h                          \
  google/protobuf/compiler/ruby/ruby_generated_code.proto      \
  google/protobuf/compiler/ruby/ruby_generated_code_pb.rb      \
  google/protobuf/compiler/ruby/ruby_generated_code_proto2.proto         \
  google/protobuf/compiler/ruby/ruby_generated_code_proto2_pb.rb         \
  google/protobuf/compiler/ruby/ruby_generated_pkg_explicit.proto        \
  google/protobuf/compiler/ruby/ruby_generated_pkg_explicit_legacy.proto \
  google/protobuf/compiler/ruby/ruby_generated_pkg_explicit_legacy_pb.rb \
  google/protobuf/compiler/ruby/ruby_generated_pkg_explicit_pb.rb        \
  google/protobuf/compiler/ruby/ruby_generated_pkg_implicit.proto        \
  google/protobuf/compiler/ruby/ruby_generated_pkg_implicit_pb.rb        \
  google/protobuf/compiler/package_info.h                      \
  google/protobuf/compiler/zip_output_unittest.sh              \
  libprotobuf-lite.map                                         \
  libprotobuf.map                                              \
  libprotoc.map                                                \
  README.md

protoc_lite_outputs =                                          \
  google/protobuf/map_lite_unittest.pb.cc                      \
  google/protobuf/map_lite_unittest.pb.h                       \
  google/protobuf/unittest_lite.pb.cc                          \
  google/protobuf/unittest_lite.pb.h                           \
  google/protobuf/unittest_no_arena_lite.pb.cc                 \
  google/protobuf/unittest_no_arena_lite.pb.h                  \
  google/protobuf/unittest_import_lite.pb.cc                   \
  google/protobuf/unittest_import_lite.pb.h                    \
  google/protobuf/unittest_import_public_lite.pb.cc            \
  google/protobuf/unittest_import_public_lite.pb.h

protoc_outputs =                                                  \
  $(protoc_lite_outputs)                                          \
  google/protobuf/any_test.pb.cc                                  \
  google/protobuf/any_test.pb.h                                   \
  google/protobuf/compiler/cpp/cpp_test_bad_identifiers.pb.cc     \
  google/protobuf/compiler/cpp/cpp_test_bad_identifiers.pb.h      \
  google/protobuf/compiler/cpp/cpp_test_large_enum_value.pb.cc    \
  google/protobuf/compiler/cpp/cpp_test_large_enum_value.pb.h     \
  google/protobuf/map_proto2_unittest.pb.cc                       \
  google/protobuf/map_proto2_unittest.pb.h                        \
  google/protobuf/map_unittest.pb.cc                              \
  google/protobuf/map_unittest.pb.h                               \
  google/protobuf/unittest_arena.pb.cc                            \
  google/protobuf/unittest_arena.pb.h                             \
  google/protobuf/unittest_custom_options.pb.cc                   \
  google/protobuf/unittest_custom_options.pb.h                    \
  google/protobuf/unittest_drop_unknown_fields.pb.cc              \
  google/protobuf/unittest_drop_unknown_fields.pb.h               \
  google/protobuf/unittest_embed_optimize_for.pb.cc               \
  google/protobuf/unittest_embed_optimize_for.pb.h                \
  google/protobuf/unittest_empty.pb.cc                            \
  google/protobuf/unittest_empty.pb.h                             \
  google/protobuf/unittest_enormous_descriptor.pb.cc              \
  google/protobuf/unittest_enormous_descriptor.pb.h               \
  google/protobuf/unittest_import.pb.cc                           \
  google/protobuf/unittest_import.pb.h                            \
  google/protobuf/unittest_import_public.pb.cc                    \
  google/protobuf/unittest_import_public.pb.h                     \
  google/protobuf/unittest_lazy_dependencies.pb.cc                \
  google/protobuf/unittest_lazy_dependencies.pb.h                 \
  google/protobuf/unittest_lazy_dependencies_custom_option.pb.cc  \
  google/protobuf/unittest_lazy_dependencies_custom_option.pb.h   \
  google/protobuf/unittest_lazy_dependencies_enum.pb.cc           \
  google/protobuf/unittest_lazy_dependencies_enum.pb.h            \
  google/protobuf/unittest_lite_imports_nonlite.pb.cc             \
  google/protobuf/unittest_lite_imports_nonlite.pb.h              \
  google/protobuf/unittest_mset.pb.cc                             \
  google/protobuf/unittest_mset.pb.h                              \
  google/protobuf/unittest_mset_wire_format.pb.cc                 \
  google/protobuf/unittest_mset_wire_format.pb.h                  \
  google/protobuf/unittest_no_arena_import.pb.cc                  \
  google/protobuf/unittest_no_arena_import.pb.h                   \
  google/protobuf/unittest_no_arena.pb.cc                         \
  google/protobuf/unittest_no_arena.pb.h                          \
  google/protobuf/unittest_no_field_presence.pb.cc                \
  google/protobuf/unittest_no_field_presence.pb.h                 \
  google/protobuf/unittest_no_generic_services.pb.cc              \
  google/protobuf/unittest_no_generic_services.pb.h               \
  google/protobuf/unittest_optimize_for.pb.cc                     \
  google/protobuf/unittest_optimize_for.pb.h                      \
  google/protobuf/unittest.pb.cc                                  \
  google/protobuf/unittest.pb.h                                   \
  google/protobuf/unittest_preserve_unknown_enum2.pb.cc           \
  google/protobuf/unittest_preserve_unknown_enum2.pb.h            \
  google/protobuf/unittest_preserve_unknown_enum.pb.cc            \
  google/protobuf/unittest_preserve_unknown_enum.pb.h             \
  google/protobuf/unittest_proto3.pb.cc                           \
  google/protobuf/unittest_proto3.pb.h                            \
  google/protobuf/unittest_proto3_arena.pb.cc                     \
  google/protobuf/unittest_proto3_arena.pb.h                      \
  google/protobuf/unittest_proto3_arena_lite.pb.cc                \
  google/protobuf/unittest_proto3_arena_lite.pb.h                 \
  google/protobuf/unittest_proto3_lite.pb.cc                      \
  google/protobuf/unittest_proto3_lite.pb.h                       \
  google/protobuf/unittest_well_known_types.pb.cc                 \
  google/protobuf/unittest_well_known_types.pb.h                  \
  google/protobuf/util/internal/testdata/anys.pb.cc               \
  google/protobuf/util/internal/testdata/anys.pb.h                \
  google/protobuf/util/internal/testdata/books.pb.cc              \
  google/protobuf/util/internal/testdata/books.pb.h               \
  google/protobuf/util/internal/testdata/default_value.pb.cc      \
  google/protobuf/util/internal/testdata/default_value.pb.h       \
  google/protobuf/util/internal/testdata/default_value_test.pb.cc \
  google/protobuf/util/internal/testdata/default_value_test.pb.h  \
  google/protobuf/util/internal/testdata/field_mask.pb.cc         \
  google/protobuf/util/internal/testdata/field_mask.pb.h          \
  google/protobuf/util/internal/testdata/maps.pb.cc               \
  google/protobuf/util/internal/testdata/maps.pb.h                \
  google/protobuf/util/internal/testdata/oneofs.pb.cc             \
  google/protobuf/util/internal/testdata/oneofs.pb.h              \
  google/protobuf/util/internal/testdata/proto3.pb.cc             \
  google/protobuf/util/internal/testdata/proto3.pb.h              \
  google/protobuf/util/internal/testdata/struct.pb.cc             \
  google/protobuf/util/internal/testdata/struct.pb.h              \
  google/protobuf/util/internal/testdata/timestamp_duration.pb.cc \
  google/protobuf/util/internal/testdata/timestamp_duration.pb.h  \
  google/protobuf/util/internal/testdata/wrappers.pb.cc           \
  google/protobuf/util/internal/testdata/wrappers.pb.h            \
  google/protobuf/util/json_format.pb.cc                          \
  google/protobuf/util/json_format.pb.h                           \
  google/protobuf/util/json_format_proto3.pb.cc                   \
  google/protobuf/util/json_format_proto3.pb.h                    \
  google/protobuf/util/message_differencer_unittest.pb.cc         \
  google/protobuf/util/message_differencer_unittest.pb.h

if USE_EXTERNAL_PROTOC

unittest_proto_middleman: $(protoc_inputs)
	$(PROTOC) -I$(srcdir) --cpp_out=. $^
	touch unittest_proto_middleman

else

# We have to cd to $(srcdir) before executing protoc because $(protoc_inputs) is
# relative to srcdir, which may not be the same as the current directory when
# building out-of-tree.
unittest_proto_middleman: protoc$(EXEEXT) $(protoc_inputs)
	oldpwd=`pwd` && ( cd $(srcdir) && $$oldpwd/protoc$(EXEEXT) -I. --cpp_out=$$oldpwd $(protoc_inputs) )
	touch unittest_proto_middleman

endif

$(protoc_outputs): unittest_proto_middleman

COMMON_TEST_SOURCES =                                          \
  google/protobuf/arena_test_util.cc                           \
  google/protobuf/arena_test_util.h                            \
  google/protobuf/map_test_util.cc                             \
  google/protobuf/map_test_util.h                              \
  google/protobuf/map_test_util_impl.h                         \
  google/protobuf/test_util.cc                                 \
  google/protobuf/test_util.h                                  \
  google/protobuf/test_util.inc                                \
  google/protobuf/test_util2.h                                 \
  google/protobuf/testing/googletest.cc                        \
  google/protobuf/testing/googletest.h                         \
  google/protobuf/testing/file.cc                              \
  google/protobuf/testing/file.h

GOOGLETEST_BUILD_DIR=../third_party/googletest/googletest
GOOGLEMOCK_BUILD_DIR=../third_party/googletest/googlemock
GOOGLETEST_SRC_DIR=$(srcdir)/../third_party/googletest/googletest
GOOGLEMOCK_SRC_DIR=$(srcdir)/../third_party/googletest/googlemock
check_PROGRAMS = protoc protobuf-test protobuf-lazy-descriptor-test \
                 protobuf-lite-test test_plugin protobuf-lite-arena-test \
                 no-warning-test $(GZCHECKPROGRAMS)
protobuf_test_LDADD = $(PTHREAD_LIBS) libprotobuf.la libprotoc.la \
                      $(GOOGLETEST_BUILD_DIR)/lib/libgtest.la     \
                      $(GOOGLEMOCK_BUILD_DIR)/lib/libgmock.la     \
                      $(GOOGLEMOCK_BUILD_DIR)/lib/libgmock_main.la
protobuf_test_CPPFLAGS = -I$(GOOGLETEST_SRC_DIR)/include \
                         -I$(GOOGLEMOCK_SRC_DIR)/include
# Disable optimization for tests unless the user explicitly asked for it,
# since test_util.cc takes forever to compile with optimization (with GCC).
# See configure.ac for more info.
protobuf_test_CXXFLAGS = $(NO_OPT_CXXFLAGS)
protobuf_test_SOURCES =                                        \
  google/protobuf/stubs/bytestream_unittest.cc                 \
  google/protobuf/stubs/common_unittest.cc                     \
  google/protobuf/stubs/int128_unittest.cc                     \
  google/protobuf/io/io_win32_unittest.cc                      \
  google/protobuf/stubs/statusor_test.cc                       \
  google/protobuf/stubs/status_test.cc                         \
  google/protobuf/stubs/stringpiece_unittest.cc                \
  google/protobuf/stubs/stringprintf_unittest.cc               \
  google/protobuf/stubs/structurally_valid_unittest.cc         \
  google/protobuf/stubs/strutil_unittest.cc                    \
  google/protobuf/stubs/template_util_unittest.cc              \
  google/protobuf/stubs/time_test.cc                           \
  google/protobuf/any_test.cc                                  \
  google/protobuf/arenastring_unittest.cc                      \
  google/protobuf/arena_unittest.cc                            \
  google/protobuf/descriptor_database_unittest.cc              \
  google/protobuf/descriptor_unittest.cc                       \
  google/protobuf/drop_unknown_fields_test.cc                  \
  google/protobuf/dynamic_message_unittest.cc                  \
  google/protobuf/extension_set_unittest.cc                    \
  google/protobuf/generated_message_reflection_unittest.cc     \
  google/protobuf/map_field_test.cc                            \
  google/protobuf/map_test.cc                                  \
  google/protobuf/message_unittest.cc                          \
  google/protobuf/message_unittest.inc                         \
  google/protobuf/no_field_presence_test.cc                    \
  google/protobuf/preserve_unknown_enum_test.cc                \
  google/protobuf/proto3_arena_lite_unittest.cc                \
  google/protobuf/proto3_arena_unittest.cc                     \
  google/protobuf/proto3_lite_unittest.cc                      \
  google/protobuf/proto3_lite_unittest.inc                     \
  google/protobuf/reflection_ops_unittest.cc                   \
  google/protobuf/repeated_field_reflection_unittest.cc        \
  google/protobuf/repeated_field_unittest.cc                   \
  google/protobuf/text_format_unittest.cc                      \
  google/protobuf/unknown_field_set_unittest.cc                \
  google/protobuf/well_known_types_unittest.cc                 \
  google/protobuf/wire_format_unittest.cc                      \
  google/protobuf/io/coded_stream_unittest.cc                  \
  google/protobuf/io/printer_unittest.cc                       \
  google/protobuf/io/tokenizer_unittest.cc                     \
  google/protobuf/io/zero_copy_stream_unittest.cc              \
  google/protobuf/compiler/annotation_test_util.h              \
  google/protobuf/compiler/annotation_test_util.cc             \
  google/protobuf/compiler/command_line_interface_unittest.cc  \
  google/protobuf/compiler/importer_unittest.cc                \
  google/protobuf/compiler/mock_code_generator.cc              \
  google/protobuf/compiler/mock_code_generator.h               \
  google/protobuf/compiler/parser_unittest.cc                  \
  google/protobuf/compiler/cpp/cpp_bootstrap_unittest.cc       \
  google/protobuf/compiler/cpp/cpp_move_unittest.cc            \
  google/protobuf/compiler/cpp/cpp_unittest.h                  \
  google/protobuf/compiler/cpp/cpp_unittest.cc                 \
  google/protobuf/compiler/cpp/cpp_unittest.inc                \
  google/protobuf/compiler/cpp/cpp_plugin_unittest.cc          \
  google/protobuf/compiler/cpp/metadata_test.cc                \
  google/protobuf/compiler/java/java_plugin_unittest.cc        \
  google/protobuf/compiler/java/java_doc_comment_unittest.cc   \
  google/protobuf/compiler/objectivec/objectivec_helpers_unittest.cc \
  google/protobuf/compiler/python/python_plugin_unittest.cc    \
  google/protobuf/compiler/ruby/ruby_generator_unittest.cc     \
  google/protobuf/compiler/csharp/csharp_bootstrap_unittest.cc \
  google/protobuf/compiler/csharp/csharp_generator_unittest.cc \
  google/protobuf/util/delimited_message_util_test.cc          \
  google/protobuf/util/field_comparator_test.cc                \
  google/protobuf/util/field_mask_util_test.cc                 \
  google/protobuf/util/internal/default_value_objectwriter_test.cc \
  google/protobuf/util/internal/json_objectwriter_test.cc      \
  google/protobuf/util/internal/json_stream_parser_test.cc     \
  google/protobuf/util/internal/protostream_objectsource_test.cc \
  google/protobuf/util/internal/protostream_objectwriter_test.cc \
  google/protobuf/util/internal/type_info_test_helper.cc       \
  google/protobuf/util/json_util_test.cc                       \
  google/protobuf/util/message_differencer_unittest.cc         \
  google/protobuf/util/time_util_test.cc                       \
  google/protobuf/util/type_resolver_util_test.cc              \
  $(COMMON_TEST_SOURCES)
nodist_protobuf_test_SOURCES = $(protoc_outputs)
$(am_protobuf_test_OBJECTS): unittest_proto_middleman

# Run cpp_unittest again with PROTOBUF_TEST_NO_DESCRIPTORS defined.
protobuf_lazy_descriptor_test_LDADD = $(PTHREAD_LIBS) libprotobuf.la \
                      libprotoc.la                                   \
                      $(GOOGLETEST_BUILD_DIR)/lib/libgtest.la        \
                      $(GOOGLEMOCK_BUILD_DIR)/lib/libgmock.la        \
                      $(GOOGLEMOCK_BUILD_DIR)/lib/libgmock_main.la
protobuf_lazy_descriptor_test_CPPFLAGS = -I$(GOOGLEMOCK_SRC_DIR)/include \
                                         -I$(GOOGLETEST_SRC_DIR)/include \
                                         -DPROTOBUF_TEST_NO_DESCRIPTORS
protobuf_lazy_descriptor_test_CXXFLAGS = $(NO_OPT_CXXFLAGS)
protobuf_lazy_descriptor_test_SOURCES =                        \
  google/protobuf/compiler/cpp/cpp_unittest.cc                 \
  $(COMMON_TEST_SOURCES)
nodist_protobuf_lazy_descriptor_test_SOURCES = $(protoc_outputs)
$(am_protobuf_lazy_descriptor_test_OBJECTS): unittest_proto_middleman

COMMON_LITE_TEST_SOURCES =                                             \
  google/protobuf/arena_test_util.cc                                   \
  google/protobuf/arena_test_util.h                                    \
  google/protobuf/map_lite_test_util.cc                                \
  google/protobuf/map_lite_test_util.h                                 \
  google/protobuf/test_util_lite.cc                                    \
  google/protobuf/test_util_lite.h

# Build lite_unittest separately, since it doesn't use gtest. It can't
# depend on gtest because our internal version of gtest depend on proto
# full runtime and we want to make sure this test builds without full
# runtime.
protobuf_lite_test_LDADD = $(PTHREAD_LIBS) libprotobuf-lite.la     \
                           $(GOOGLETEST_BUILD_DIR)/lib/libgtest.la \
                           $(GOOGLEMOCK_BUILD_DIR)/lib/libgmock.la \
                           $(GOOGLEMOCK_BUILD_DIR)/lib/libgmock_main.la
protobuf_lite_test_CPPFLAGS= -I$(GOOGLEMOCK_SRC_DIR)/include \
                             -I$(GOOGLETEST_SRC_DIR)/include
protobuf_lite_test_CXXFLAGS = $(NO_OPT_CXXFLAGS)
protobuf_lite_test_SOURCES =                                           \
  google/protobuf/lite_unittest.cc                                     \
  $(COMMON_LITE_TEST_SOURCES)
nodist_protobuf_lite_test_SOURCES = $(protoc_lite_outputs)
$(am_protobuf_lite_test_OBJECTS): unittest_proto_middleman

# lite_arena_unittest depends on gtest because teboring@ found that without
# gtest when building the test internally our memory sanitizer doesn't detect
# memory leaks (don't know why).
protobuf_lite_arena_test_LDADD = $(PTHREAD_LIBS) libprotobuf-lite.la \
                      $(GOOGLETEST_BUILD_DIR)/lib/libgtest.la        \
                      $(GOOGLEMOCK_BUILD_DIR)/lib/libgmock.la        \
                      $(GOOGLEMOCK_BUILD_DIR)/lib/libgmock_main.la
protobuf_lite_arena_test_CPPFLAGS = -I$(GOOGLEMOCK_SRC_DIR)/include  \
                                    -I$(GOOGLETEST_SRC_DIR)/include
protobuf_lite_arena_test_CXXFLAGS = $(NO_OPT_CXXFLAGS)
protobuf_lite_arena_test_SOURCES =       \
  google/protobuf/lite_arena_unittest.cc \
  $(COMMON_LITE_TEST_SOURCES)
nodist_protobuf_lite_arena_test_SOURCES = $(protoc_lite_outputs)
$(am_protobuf_lite_arena_test_OBJECTS): unittest_proto_middleman

# Test plugin binary.
test_plugin_LDADD = $(PTHREAD_LIBS) libprotobuf.la libprotoc.la \
                    $(GOOGLETEST_BUILD_DIR)/lib/libgtest.la
test_plugin_CPPFLAGS = -I$(GOOGLETEST_SRC_DIR)/include
test_plugin_SOURCES =                                          \
  google/protobuf/compiler/mock_code_generator.cc              \
  google/protobuf/testing/file.cc                              \
  google/protobuf/testing/file.h                               \
  google/protobuf/compiler/test_plugin.cc

if HAVE_ZLIB
zcgzip_LDADD = $(PTHREAD_LIBS) libprotobuf.la
zcgzip_SOURCES = google/protobuf/testing/zcgzip.cc

zcgunzip_LDADD = $(PTHREAD_LIBS) libprotobuf.la
zcgunzip_SOURCES = google/protobuf/testing/zcgunzip.cc
endif

# This test target is to ensure all our public header files and generated
# code is free from warnings. We have to be more pedantic about these
# files because they are compiled by users with different compiler flags.
no_warning_test.cc:
	echo "// Generated from Makefile.am" > no_warning_test.cc
	for FILE in $(nobase_include_HEADERS); do \
    echo "#include <$${FILE}>" >> no_warning_test.cc; \
	done
	echo "int main(int, char**) { return 0; }" >> no_warning_test.cc

no_warning_test_LDADD = $(PTHREAD_LIBS) libprotobuf.la libprotoc.la
no_warning_test_CXXFLAGS = $(PTHREAD_CFLAGS) $(PTHREAD_DEF) $(ZLIB_DEF) \
                           -Wall -Wextra -Werror -Wno-unused-parameter
nodist_no_warning_test_SOURCES = no_warning_test.cc $(protoc_outputs)

TESTS = protobuf-test protobuf-lazy-descriptor-test protobuf-lite-test \
        google/protobuf/compiler/zip_output_unittest.sh $(GZTESTS)     \
        protobuf-lite-arena-test no-warning-test
