## Process this file with automake to produce Makefile.in

conformance_protoc_inputs =                                    \
  conformance.proto                                            \
  $(top_srcdir)/src/google/protobuf/test_messages_proto3.proto

# proto2 input files, should be separated with proto3, as we
# can't generate proto2 files for ruby, php and objc
conformance_proto2_protoc_inputs =                             \
  $(top_srcdir)/src/google/protobuf/test_messages_proto2.proto

well_known_type_protoc_inputs =                                \
  $(top_srcdir)/src/google/protobuf/any.proto                  \
  $(top_srcdir)/src/google/protobuf/duration.proto             \
  $(top_srcdir)/src/google/protobuf/field_mask.proto           \
  $(top_srcdir)/src/google/protobuf/struct.proto               \
  $(top_srcdir)/src/google/protobuf/timestamp.proto            \
  $(top_srcdir)/src/google/protobuf/wrappers.proto


protoc_outputs =                                               \
  conformance.pb.cc                                            \
  conformance.pb.h

other_language_protoc_outputs =                                \
  conformance_pb2.py                                           \
  Conformance.pbobjc.h                                         \
  Conformance.pbobjc.m                                         \
  conformance_pb.js                                            \
  conformance_pb.rb                                            \
  com/google/protobuf/Any.java                                 \
  com/google/protobuf/AnyOrBuilder.java                        \
  com/google/protobuf/AnyProto.java                            \
  com/google/protobuf/BoolValue.java                           \
  com/google/protobuf/BoolValueOrBuilder.java                  \
  com/google/protobuf/BytesValue.java                          \
  com/google/protobuf/BytesValueOrBuilder.java                 \
  com/google/protobuf/conformance/Conformance.java             \
  com/google/protobuf/DoubleValue.java                         \
  com/google/protobuf/DoubleValueOrBuilder.java                \
  com/google/protobuf/Duration.java                            \
  com/google/protobuf/DurationOrBuilder.java                   \
  com/google/protobuf/DurationProto.java                       \
  com/google/protobuf/FieldMask.java                           \
  com/google/protobuf/FieldMaskOrBuilder.java                  \
  com/google/protobuf/FieldMaskProto.java                      \
  com/google/protobuf/FloatValue.java                          \
  com/google/protobuf/FloatValueOrBuilder.java                 \
  com/google/protobuf/Int32Value.java                          \
  com/google/protobuf/Int32ValueOrBuilder.java                 \
  com/google/protobuf/Int64Value.java                          \
  com/google/protobuf/Int64ValueOrBuilder.java                 \
  com/google/protobuf/ListValue.java                           \
  com/google/protobuf/ListValueOrBuilder.java                  \
  com/google/protobuf/NullValue.java                           \
  com/google/protobuf/StringValue.java                         \
  com/google/protobuf/StringValueOrBuilder.java                \
  com/google/protobuf/Struct.java                              \
  com/google/protobuf/StructOrBuilder.java                     \
  com/google/protobuf/StructProto.java                         \
  com/google/protobuf/Timestamp.java                           \
  com/google/protobuf/TimestampOrBuilder.java                  \
  com/google/protobuf/TimestampProto.java                      \
  com/google/protobuf/UInt32Value.java                         \
  com/google/protobuf/UInt32ValueOrBuilder.java                \
  com/google/protobuf/UInt64Value.java                         \
  com/google/protobuf/UInt64ValueOrBuilder.java                \
  com/google/protobuf/Value.java                               \
  com/google/protobuf/ValueOrBuilder.java                      \
  com/google/protobuf/WrappersProto.java                       \
  com/google/protobuf_test_messages/proto3/TestMessagesProto3.java \
  com/google/protobuf_test_messages/proto2/TestMessagesProto2.java \
  google/protobuf/any.pb.cc                                    \
  google/protobuf/any.pb.h                                     \
  google/protobuf/any.rb                                       \
  google/protobuf/any_pb2.py                                   \
  google/protobuf/duration.pb.cc                               \
  google/protobuf/duration.pb.h                                \
  google/protobuf/duration.rb                                  \
  google/protobuf/duration_pb2.py                              \
  google/protobuf/field_mask.pb.cc                             \
  google/protobuf/field_mask.pb.h                              \
  google/protobuf/field_mask.rb                                \
  google/protobuf/field_mask_pb2.py                            \
  google/protobuf/struct.pb.cc                                 \
  google/protobuf/struct.pb.h                                  \
  google/protobuf/struct.rb                                    \
  google/protobuf/struct_pb2.py                                \
  google/protobuf/TestMessagesProto2.pbobjc.h                  \
  google/protobuf/TestMessagesProto2.pbobjc.m                  \
  google/protobuf/TestMessagesProto3.pbobjc.h                  \
  google/protobuf/TestMessagesProto3.pbobjc.m                  \
  google/protobuf/test_messages_proto3.pb.cc                   \
  google/protobuf/test_messages_proto3.pb.h                    \
  google/protobuf/test_messages_proto2.pb.cc                   \
  google/protobuf/test_messages_proto2.pb.h                    \
  google/protobuf/test_messages_proto3_pb.rb                   \
  google/protobuf/test_messages_proto3_pb2.py                  \
  google/protobuf/test_messages_proto2_pb2.py                  \
  google/protobuf/timestamp.pb.cc                              \
  google/protobuf/timestamp.pb.h                               \
  google/protobuf/timestamp.rb                                 \
  google/protobuf/timestamp_pb2.py                             \
  google/protobuf/wrappers.pb.cc                               \
  google/protobuf/wrappers.pb.h                                \
  google/protobuf/wrappers.rb                                  \
  google/protobuf/wrappers_pb2.py                              \
  Conformance/ConformanceRequest.php                           \
  Conformance/ConformanceResponse.php                          \
  Conformance/FailureSet.php                                   \
  Conformance/WireFormat.php                                   \
  GPBMetadata/Conformance.php                                  \
  GPBMetadata/Google/Protobuf/Any.php                          \
  GPBMetadata/Google/Protobuf/Duration.php                     \
  GPBMetadata/Google/Protobuf/FieldMask.php                    \
  GPBMetadata/Google/Protobuf/Struct.php                       \
  GPBMetadata/Google/Protobuf/TestMessagesProto3.php           \
  GPBMetadata/Google/Protobuf/Timestamp.php                    \
  GPBMetadata/Google/Protobuf/Wrappers.php                     \
  Google/Protobuf/Any.php                                      \
  Google/Protobuf/BoolValue.php                                \
  Google/Protobuf/BytesValue.php                               \
  Google/Protobuf/DoubleValue.php                              \
  Google/Protobuf/Duration.php                                 \
  Google/Protobuf/FieldMask.php                                \
  Google/Protobuf/FloatValue.php                               \
  Google/Protobuf/Int32Value.php                               \
  Google/Protobuf/Int64Value.php                               \
  Google/Protobuf/ListValue.php                                \
  Google/Protobuf/NullValue.php                                \
  Google/Protobuf/StringValue.php                              \
  Google/Protobuf/Struct.php                                   \
  Google/Protobuf/Timestamp.php                                \
  Google/Protobuf/UInt32Value.php                              \
  Google/Protobuf/UInt64Value.php                              \
  Google/Protobuf/Value.php                                    \
  Protobuf_test_messages/Proto3/ForeignEnum.php                \
  Protobuf_test_messages/Proto3/ForeignMessage.php             \
  Protobuf_test_messages/Proto3/TestAllTypes_NestedEnum.php    \
  Protobuf_test_messages/Proto3/TestAllTypes_NestedMessage.php \
  Protobuf_test_messages/Proto3/TestAllTypes.php
  # lite/com/google/protobuf/Any.java                            \
  # lite/com/google/protobuf/AnyOrBuilder.java                   \
  # lite/com/google/protobuf/AnyProto.java                       \
  # lite/com/google/protobuf/BoolValue.java                      \
  # lite/com/google/protobuf/BoolValueOrBuilder.java             \
  # lite/com/google/protobuf/BytesValue.java                     \
  # lite/com/google/protobuf/BytesValueOrBuilder.java            \
  # lite/com/google/protobuf/conformance/Conformance.java        \
  # lite/com/google/protobuf/DoubleValue.java                    \
  # lite/com/google/protobuf/DoubleValueOrBuilder.java           \
  # lite/com/google/protobuf/Duration.java                       \
  # lite/com/google/protobuf/DurationOrBuilder.java              \
  # lite/com/google/protobuf/DurationProto.java                  \
  # lite/com/google/protobuf/FieldMask.java                      \
  # lite/com/google/protobuf/FieldMaskOrBuilder.java             \
  # lite/com/google/protobuf/FieldMaskProto.java                 \
  # lite/com/google/protobuf/FloatValue.java                     \
  # lite/com/google/protobuf/FloatValueOrBuilder.java            \
  # lite/com/google/protobuf/Int32Value.java                     \
  # lite/com/google/protobuf/Int32ValueOrBuilder.java            \
  # lite/com/google/protobuf/Int64Value.java                     \
  # lite/com/google/protobuf/Int64ValueOrBuilder.java            \
  # lite/com/google/protobuf/ListValue.java                      \
  # lite/com/google/protobuf/ListValueOrBuilder.java             \
  # lite/com/google/protobuf/NullValue.java                      \
  # lite/com/google/protobuf/StringValue.java                    \
  # lite/com/google/protobuf/StringValueOrBuilder.java           \
  # lite/com/google/protobuf/Struct.java                         \
  # lite/com/google/protobuf/StructOrBuilder.java                \
  # lite/com/google/protobuf/StructProto.java                    \
  # lite/com/google/protobuf/Timestamp.java                      \
  # lite/com/google/protobuf/TimestampOrBuilder.java             \
  # lite/com/google/protobuf/TimestampProto.java                 \
  # lite/com/google/protobuf/UInt32Value.java                    \
  # lite/com/google/protobuf/UInt32ValueOrBuilder.java           \
  # lite/com/google/protobuf/UInt64Value.java                    \
  # lite/com/google/protobuf/UInt64ValueOrBuilder.java           \
  # lite/com/google/protobuf/Value.java                          \
  # lite/com/google/protobuf/ValueOrBuilder.java                 \
  # lite/com/google/protobuf/WrappersProto.java

bin_PROGRAMS = conformance-test-runner conformance-cpp

# All source files excepet C++/Objective-C ones should be explicitly listed
# here because the autoconf tools don't include files of other languages
# automatically.
EXTRA_DIST =                  \
  ConformanceJava.java        \
  ConformanceJavaLite.java    \
  README.md                   \
  conformance.proto           \
  conformance_python.py       \
  conformance_ruby.rb         \
  conformance_php.php         \
  failure_list_cpp.txt        \
  failure_list_csharp.txt     \
  failure_list_java.txt       \
  failure_list_js.txt         \
  failure_list_objc.txt       \
  failure_list_python.txt     \
  failure_list_python_cpp.txt \
  failure_list_python-post26.txt \
  failure_list_ruby.txt       \
  failure_list_php.txt        \
  failure_list_php_c.txt

conformance_test_runner_LDADD = $(top_srcdir)/src/libprotobuf.la
conformance_test_runner_SOURCES = conformance_test.h conformance_test.cc \
                                  conformance_test_main.cc               \
                                  binary_json_conformance_suite.h        \
                                  binary_json_conformance_suite.cc       \
                                  text_format_conformance_suite.h        \
                                  text_format_conformance_suite.cc       \
                                  conformance_test_runner.cc             \
                                  third_party/jsoncpp/json.h             \
                                  third_party/jsoncpp/jsoncpp.cpp
nodist_conformance_test_runner_SOURCES = conformance.pb.cc google/protobuf/test_messages_proto3.pb.cc google/protobuf/test_messages_proto2.pb.cc
conformance_test_runner_CPPFLAGS = -I$(top_srcdir)/src -I$(srcdir)
conformance_test_runner_CXXFLAGS = -std=c++11
# Explicit deps beacuse BUILT_SOURCES are only done before a "make all/check"
# so a direct "make test_cpp" could fail if parallel enough.
conformance_test_runner-conformance_test.$(OBJEXT): conformance.pb.h
conformance_test_runner-conformance_test_runner.$(OBJEXT): conformance.pb.h

conformance_cpp_LDADD = $(top_srcdir)/src/libprotobuf.la
conformance_cpp_SOURCES = conformance_cpp.cc
nodist_conformance_cpp_SOURCES = conformance.pb.cc google/protobuf/test_messages_proto3.pb.cc google/protobuf/test_messages_proto2.pb.cc
conformance_cpp_CPPFLAGS = -I$(top_srcdir)/src
# Explicit dep beacuse BUILT_SOURCES are only done before a "make all/check"
# so a direct "make test_cpp" could fail if parallel enough.
conformance_cpp-conformance_cpp.$(OBJEXT): conformance.pb.h

if OBJC_CONFORMANCE_TEST

bin_PROGRAMS += conformance-objc

conformance_objc_SOURCES = conformance_objc.m ../objectivec/GPBProtocolBuffers.m
nodist_conformance_objc_SOURCES = Conformance.pbobjc.m google/protobuf/TestMessagesProto2.pbobjc.m google/protobuf/TestMessagesProto3.pbobjc.m
# On travis, the build fails without the isysroot because whatever system
# headers are being found don't include generics support for
# NSArray/NSDictionary, the only guess is their image at one time had an odd
# setup for Xcode and old frameworks are being found.
conformance_objc_CPPFLAGS = -I$(top_srcdir)/objectivec -isysroot `xcrun --sdk macosx --show-sdk-path`
conformance_objc_LDFLAGS = -framework Foundation
# Explicit dep beacuse BUILT_SOURCES are only done before a "make all/check"
# so a direct "make test_objc" could fail if parallel enough.
conformance_objc-conformance_objc.$(OBJEXT): Conformance.pbobjc.h google/protobuf/TestMessagesProto2.pbobjc.h google/protobuf/TestMessagesProto3.pbobjc.h

endif

# JavaScript well-known types are expected to be in a directory called
# google-protobuf, because they are usually in the google-protobuf npm
# package.  But we want to use the sources from our tree, so we recreate
# that directory structure here.
google-protobuf:
	mkdir google-protobuf

if USE_EXTERNAL_PROTOC

# Some implementations include pre-generated versions of well-known types.
protoc_middleman: $(conformance_protoc_inputs) $(conformance_proto2_protoc_inputs) $(well_known_type_protoc_inputs) google-protobuf
	$(PROTOC) -I$(srcdir) -I$(top_srcdir) --cpp_out=. --java_out=. --ruby_out=. --objc_out=. --python_out=. --php_out=. --js_out=import_style=commonjs,binary:. $(conformance_protoc_inputs)
	$(PROTOC) -I$(srcdir) -I$(top_srcdir) --cpp_out=. --java_out=. --objc_out=. --python_out=. --js_out=import_style=commonjs,binary:. $(conformance_proto2_protoc_inputs)
	$(PROTOC) -I$(srcdir) -I$(top_srcdir) --cpp_out=. --java_out=. --ruby_out=. --python_out=. --js_out=import_style=commonjs,binary:google-protobuf $(well_known_type_protoc_inputs)
	## $(PROTOC) -I$(srcdir) -I$(top_srcdir) --java_out=lite:lite $(conformance_protoc_inputs) $(well_known_type_protoc_inputs)
	touch protoc_middleman

else

# We have to cd to $(srcdir) before executing protoc because $(protoc_inputs) is
# relative to srcdir, which may not be the same as the current directory when
# building out-of-tree.
protoc_middleman: $(top_srcdir)/src/protoc$(EXEEXT) $(conformance_protoc_inputs) $(conformance_proto2_protoc_inputs) $(well_known_type_protoc_inputs) google-protobuf
	oldpwd=`pwd` && ( cd $(srcdir) && $$oldpwd/../src/protoc$(EXEEXT) -I. -I$(top_srcdir)/src --cpp_out=$$oldpwd --java_out=$$oldpwd --ruby_out=$$oldpwd --objc_out=$$oldpwd --python_out=$$oldpwd --php_out=$$oldpwd --js_out=import_style=commonjs,binary:$$oldpwd $(conformance_protoc_inputs) )
	oldpwd=`pwd` && ( cd $(srcdir) && $$oldpwd/../src/protoc$(EXEEXT) -I. -I$(top_srcdir)/src --cpp_out=$$oldpwd --java_out=$$oldpwd --objc_out=. --python_out=$$oldpwd --js_out=import_style=commonjs,binary:$$oldpwd $(conformance_proto2_protoc_inputs) )
	oldpwd=`pwd` && ( cd $(srcdir) && $$oldpwd/../src/protoc$(EXEEXT) -I. -I$(top_srcdir)/src --cpp_out=$$oldpwd --java_out=$$oldpwd --ruby_out=$$oldpwd --python_out=$$oldpwd --js_out=import_style=commonjs,binary:$$oldpwd/google-protobuf $(well_known_type_protoc_inputs) )
	## @mkdir -p lite
	## oldpwd=`pwd` && ( cd $(srcdir) && $$oldpwd/../src/protoc$(EXEEXT) -I. -I$(top_srcdir)/src --java_out=lite:$$oldpwd/lite $(conformance_protoc_inputs) $(well_known_type_protoc_inputs) )
	touch protoc_middleman

endif

$(protoc_outputs): protoc_middleman

$(other_language_protoc_outputs): protoc_middleman

CLEANFILES = $(protoc_outputs) protoc_middleman javac_middleman conformance-java javac_middleman_lite conformance-java-lite conformance-csharp conformance-php conformance-php-c $(other_language_protoc_outputs)

MAINTAINERCLEANFILES =   \
  Makefile.in

javac_middleman: ConformanceJava.java protoc_middleman $(other_language_protoc_outputs)
	jar=`ls ../java/util/target/*jar-with-dependencies.jar` && javac -classpath ../java/target/classes:$$jar ConformanceJava.java com/google/protobuf/conformance/Conformance.java com/google/protobuf_test_messages/proto3/TestMessagesProto3.java com/google/protobuf_test_messages/proto2/TestMessagesProto2.java
	@touch javac_middleman

conformance-java: javac_middleman
	@echo "Writing shortcut script conformance-java..."
	@echo '#! /bin/sh' > conformance-java
	@jar=`ls ../java/util/target/*jar-with-dependencies.jar` && echo java -classpath .:../java/target/classes:$$jar ConformanceJava '$$@' >> conformance-java
	@chmod +x conformance-java

javac_middleman_lite: ConformanceJavaLite.java protoc_middleman $(other_language_protoc_outputs)
	javac -classpath ../java/lite/target/classes:lite ConformanceJavaLite.java lite/com/google/protobuf/conformance/Conformance.java
	@touch javac_middleman_lite

conformance-java-lite: javac_middleman_lite
	@echo "Writing shortcut script conformance-java-lite..."
	@echo '#! /bin/sh' > conformance-java-lite
	@echo java -classpath .:../java/lite/target/classes:lite ConformanceJavaLite '$$@' >> conformance-java-lite
	@chmod +x conformance-java-lite

# Currently the conformance code is alongside the rest of the C#
# source, as it's easier to maintain there. We assume we've already
# built that, so we just need a script to run it.
conformance-csharp: $(other_language_protoc_outputs)
	@echo "Writing shortcut script conformance-csharp..."
	@echo '#! /bin/sh' > conformance-csharp
	@echo 'dotnet ../csharp/src/Google.Protobuf.Conformance/bin/Release/netcoreapp2.1/Google.Protobuf.Conformance.dll "$$@"' >> conformance-csharp
	@chmod +x conformance-csharp

conformance-php:
	@echo "Writing shortcut script conformance-php..."
	@echo '#! /bin/sh' > conformance-php
	@echo 'php -d auto_prepend_file=autoload.php ./conformance_php.php' >> conformance-php
	@chmod +x conformance-php

conformance-php-c:
	@echo "Writing shortcut script conformance-php-c..."
	@echo '#! /bin/sh' > conformance-php-c
	@echo 'php -dextension=../php/ext/google/protobuf/modules/protobuf.so ./conformance_php.php' >> conformance-php-c
	@chmod +x conformance-php-c

# Targets for actually running tests.
test_cpp: protoc_middleman conformance-test-runner conformance-cpp
	./conformance-test-runner --enforce_recommended --failure_list failure_list_cpp.txt ./conformance-cpp

test_java: protoc_middleman conformance-test-runner conformance-java
	./conformance-test-runner --enforce_recommended --failure_list failure_list_java.txt --text_format_failure_list text_format_failure_list_java.txt ./conformance-java

test_java_lite: protoc_middleman conformance-test-runner conformance-java-lite
	./conformance-test-runner --enforce_recommended ./conformance-java-lite

test_csharp: protoc_middleman conformance-test-runner conformance-csharp
	./conformance-test-runner --enforce_recommended --failure_list failure_list_csharp.txt --text_format_failure_list text_format_failure_list_csharp.txt ./conformance-csharp

test_ruby: protoc_middleman conformance-test-runner $(other_language_protoc_outputs)
	RUBYLIB=../ruby/lib:. ./conformance-test-runner --enforce_recommended --failure_list failure_list_ruby.txt --text_format_failure_list text_format_failure_list_ruby.txt ./conformance_ruby.rb

test_ruby_mac: protoc_middleman conformance-test-runner $(other_language_protoc_outputs)
	RUBYLIB=../ruby/lib:. ./conformance-test-runner --enforce_recommended --failure_list failure_list_ruby_mac.txt --text_format_failure_list text_format_failure_list_ruby.txt ./conformance_ruby.rb

test_php: protoc_middleman conformance-test-runner conformance-php $(other_language_protoc_outputs)
	./conformance-test-runner --enforce_recommended --failure_list failure_list_php.txt --text_format_failure_list text_format_failure_list_php.txt ./conformance-php

test_php_c: protoc_middleman conformance-test-runner conformance-php-c $(other_language_protoc_outputs)
	./conformance-test-runner --enforce_recommended --failure_list failure_list_php_c.txt --text_format_failure_list text_format_failure_list_php.txt ./conformance-php-c

test_php_zts_c: protoc_middleman conformance-test-runner conformance-php-c $(other_language_protoc_outputs)
	./conformance-test-runner --enforce_recommended --failure_list failure_list_php_zts_c.txt --text_format_failure_list text_format_failure_list_php.txt ./conformance-php-c

# These depend on library paths being properly set up.  The easiest way to
# run them is to just use "tox" from the python dir.
test_python: protoc_middleman conformance-test-runner
	./conformance-test-runner --enforce_recommended --failure_list failure_list_python.txt --text_format_failure_list text_format_failure_list_python.txt ./conformance_python.py

test_python_cpp: protoc_middleman conformance-test-runner
	./conformance-test-runner --enforce_recommended --failure_list failure_list_python_cpp.txt ./conformance_python.py

test_nodejs: protoc_middleman conformance-test-runner $(other_language_protoc_outputs)
	NODE_PATH=../js:. ./conformance-test-runner --enforce_recommended --failure_list failure_list_js.txt ./conformance_nodejs.js

if OBJC_CONFORMANCE_TEST

test_objc: protoc_middleman conformance-test-runner conformance-objc
	./conformance-test-runner --enforce_recommended --failure_list failure_list_objc.txt ./conformance-objc

endif
