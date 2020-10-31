benchmarks_protoc_inputs_benchmark_wrapper =                               \
	benchmarks.proto

benchmarks_protoc_inputs =                                                 \
	datasets/google_message1/proto3/benchmark_message1_proto3.proto

benchmarks_protoc_inputs_proto2 =                                          \
	datasets/google_message1/proto2/benchmark_message1_proto2.proto          \
	datasets/google_message2/benchmark_message2.proto                        \
	datasets/google_message3/benchmark_message3.proto                        \
	datasets/google_message3/benchmark_message3_1.proto                      \
	datasets/google_message3/benchmark_message3_2.proto                      \
	datasets/google_message3/benchmark_message3_3.proto                      \
	datasets/google_message3/benchmark_message3_4.proto                      \
	datasets/google_message3/benchmark_message3_5.proto                      \
	datasets/google_message3/benchmark_message3_6.proto                      \
	datasets/google_message3/benchmark_message3_7.proto                      \
	datasets/google_message3/benchmark_message3_8.proto                      \
	datasets/google_message4/benchmark_message4.proto                        \
	datasets/google_message4/benchmark_message4_1.proto                      \
	datasets/google_message4/benchmark_message4_2.proto                      \
	datasets/google_message4/benchmark_message4_3.proto

make_tmp_dir:
	mkdir -p 'tmp/java/src/main/java'
	touch make_tmp_dir


# We have to cd to $(srcdir) before executing protoc because $(protoc_inputs) is
# relative to srcdir, which may not be the same as the current directory when
# building out-of-tree.
protoc_middleman: make_tmp_dir $(top_srcdir)/src/protoc$(EXEEXT) $(benchmarks_protoc_inputs) $(well_known_type_protoc_inputs) $(benchmarks_protoc_inputs_benchmark_wrapper)
	oldpwd=`pwd` && ( cd $(srcdir) && $$oldpwd/../src/protoc$(EXEEXT) -I. -I$(top_srcdir)/src --cpp_out=$$oldpwd/cpp --java_out=$$oldpwd/tmp/java/src/main/java --python_out=$$oldpwd/tmp $(benchmarks_protoc_inputs) $(benchmarks_protoc_inputs_benchmark_wrapper) )
	touch protoc_middleman

protoc_middleman2:  make_tmp_dir $(top_srcdir)/src/protoc$(EXEEXT) $(benchmarks_protoc_inputs_proto2) $(well_known_type_protoc_inputs)
	oldpwd=`pwd` && ( cd $(srcdir) && $$oldpwd/../src/protoc$(EXEEXT) -I. -I$(top_srcdir)/src --cpp_out=$$oldpwd/cpp --java_out=$$oldpwd/tmp/java/src/main/java --python_out=$$oldpwd/tmp $(benchmarks_protoc_inputs_proto2) )
	touch protoc_middleman2

all_data = $$(find $$(cd $(srcdir) && pwd) -type f -name "dataset.*.pb" -not -path "$$(cd $(srcdir) && pwd)/tmp/*")

############# CPP RULES ##############

benchmarks_protoc_outputs =                                                \
	cpp/benchmarks.pb.cc                                                     \
	cpp/datasets/google_message1/proto3/benchmark_message1_proto3.pb.cc

benchmarks_protoc_outputs_header =                                         \
	cpp/benchmarks.pb.h                                                      \
	cpp/datasets/google_message1/proto3/benchmark_message1_proto3.pb.h

benchmarks_protoc_outputs_proto2_header =                                  \
	cpp/datasets/google_message1/proto2/benchmark_message1_proto2.pb.h       \
	cpp/datasets/google_message2/benchmark_message2.pb.h                     \
	cpp/datasets/google_message3/benchmark_message3.pb.h                     \
	cpp/datasets/google_message3/benchmark_message3_1.pb.h                   \
	cpp/datasets/google_message3/benchmark_message3_2.pb.h                   \
	cpp/datasets/google_message3/benchmark_message3_3.pb.h                   \
	cpp/datasets/google_message3/benchmark_message3_4.pb.h                   \
	cpp/datasets/google_message3/benchmark_message3_5.pb.h                   \
	cpp/datasets/google_message3/benchmark_message3_6.pb.h                   \
	cpp/datasets/google_message3/benchmark_message3_7.pb.h                   \
	cpp/datasets/google_message3/benchmark_message3_8.pb.h                   \
	cpp/datasets/google_message4/benchmark_message4.pb.h                     \
	cpp/datasets/google_message4/benchmark_message4_1.pb.h                   \
	cpp/datasets/google_message4/benchmark_message4_2.pb.h                   \
	cpp/datasets/google_message4/benchmark_message4_3.pb.h

benchmarks_protoc_outputs_proto2 =                                         \
	cpp/datasets/google_message1/proto2/benchmark_message1_proto2.pb.cc      \
	cpp/datasets/google_message2/benchmark_message2.pb.cc                    \
	cpp/datasets/google_message3/benchmark_message3.pb.cc                    \
	cpp/datasets/google_message3/benchmark_message3_1.pb.cc                  \
	cpp/datasets/google_message3/benchmark_message3_2.pb.cc                  \
	cpp/datasets/google_message3/benchmark_message3_3.pb.cc                  \
	cpp/datasets/google_message3/benchmark_message3_4.pb.cc                  \
	cpp/datasets/google_message3/benchmark_message3_5.pb.cc                  \
	cpp/datasets/google_message3/benchmark_message3_6.pb.cc                  \
	cpp/datasets/google_message3/benchmark_message3_7.pb.cc                  \
	cpp/datasets/google_message3/benchmark_message3_8.pb.cc                  \
	cpp/datasets/google_message4/benchmark_message4.pb.cc                    \
	cpp/datasets/google_message4/benchmark_message4_1.pb.cc                  \
	cpp/datasets/google_message4/benchmark_message4_2.pb.cc                  \
	cpp/datasets/google_message4/benchmark_message4_3.pb.cc


$(benchmarks_protoc_outputs): protoc_middleman
$(benchmarks_protoc_outputs_header): protoc_middleman
$(benchmarks_protoc_outputs_proto2): protoc_middleman2
$(benchmarks_protoc_outputs_proto2_header): protoc_middleman2

initialize_submodule:
	oldpwd=`pwd`
	cd $(top_srcdir) && git submodule update --init -r third_party/benchmark && \
		cd third_party/benchmark && cmake -DCMAKE_BUILD_TYPE=Release && make
	cd $$oldpwd
	touch initialize_submodule

$(top_srcdir)/third_party/benchmark/src/libbenchmark.a: initialize_submodule

AM_CXXFLAGS = $(NO_OPT_CXXFLAGS) $(PROTOBUF_OPT_FLAG) -Wall -Wwrite-strings -Woverloaded-virtual -Wno-sign-compare

bin_PROGRAMS = cpp-benchmark

cpp_benchmark_LDADD = $(top_srcdir)/src/libprotobuf.la $(top_srcdir)/third_party/benchmark/src/libbenchmark.a
cpp_benchmark_SOURCES = cpp/cpp_benchmark.cc
cpp_benchmark_CPPFLAGS = -I$(top_srcdir)/src -I$(srcdir)/cpp -I$(top_srcdir)/third_party/benchmark/include
# Explicit deps because BUILT_SOURCES are only done before a "make all/check"
# so a direct "make test_cpp" could fail if parallel enough.
# See: https://www.gnu.org/software/automake/manual/html_node/Built-Sources-Example.html#Recording-Dependencies-manually
cpp/cpp_benchmark-cpp_benchmark.$(OBJEXT): $(benchmarks_protoc_outputs) $(benchmarks_protoc_outputs_proto2) $(benchmarks_protoc_outputs_header) $(benchmarks_protoc_outputs_proto2_header) $(top_srcdir)/src/libprotobuf.la $(top_srcdir)/third_party/benchmark/src/libbenchmark.a
cpp/benchmark-cpp_benchmark.$(OBJEXT): $(benchmarks_protoc_outputs) $(benchmarks_protoc_outputs_proto2) $(benchmarks_protoc_outputs_header) $(benchmarks_protoc_outputs_proto2_header) $(top_srcdir)/src/libprotobuf.la $(top_srcdir)/third_party/benchmark/src/libbenchmark.a
nodist_cpp_benchmark_SOURCES =                                             \
	$(benchmarks_protoc_outputs)                                             \
	$(benchmarks_protoc_outputs_proto2)                                      \
	$(benchmarks_protoc_outputs_proto2_header)                               \
	$(benchmarks_protoc_outputs_header)

cpp: protoc_middleman protoc_middleman2 cpp-benchmark initialize_submodule
	./cpp-benchmark $(all_data)

############ CPP RULES END ############

############# JAVA RULES ##############

java_benchmark_testing_files =                                      \
	java/src/main/java/com/google/protobuf/ProtoCaliperBenchmark.java

javac_middleman: $(java_benchmark_testing_files) protoc_middleman protoc_middleman2
	cp -r $(srcdir)/java tmp
	mkdir -p tmp/java/lib
	cp $(top_srcdir)/java/core/target/*.jar tmp/java/lib/protobuf-java.jar
	cd tmp/java && mvn clean compile assembly:single -Dprotobuf.version=$(PACKAGE_VERSION) && cd ../..
	@touch javac_middleman

java-benchmark: javac_middleman
	@echo "Writing shortcut script java-benchmark..."
	@echo '#! /bin/bash' > java-benchmark
	@echo 'all_data=""' >> java-benchmark
	@echo 'conf=()' >> java-benchmark
	@echo 'data_files=""' >> java-benchmark
	@echo 'for arg in $$@; do if [[ $${arg:0:1} == "-" ]]; then conf+=($$arg); else data_files+="$$arg,"; fi; done' >> java-benchmark
	@echo 'java -cp '\"tmp/java/target/*:$(top_srcdir)/java/core/target/*:$(top_srcdir)/java/util/target/*\"" \\" >>java-benchmark
	@echo '   com.google.caliper.runner.CaliperMain com.google.protobuf.ProtoCaliperBenchmark -i runtime '"\\"  >> java-benchmark
	@echo '   -b serializeToByteArray,serializeToMemoryStream,deserializeFromByteArray,deserializeFromMemoryStream '"\\" >> java-benchmark
	@echo '   -DdataFile=$${data_files:0:-1} $${conf[*]}' >> java-benchmark
	@chmod +x java-benchmark

java: protoc_middleman protoc_middleman2 java-benchmark
	./java-benchmark $(all_data)

############# JAVA RULES END ##############


############# PYTHON RULES ##############

python_add_init: protoc_middleman protoc_middleman2
	all_file=`find tmp -type f -regex '.*\.py'` &&                   \
	for file in $${all_file[@]}; do                                  \
		path="$${file%/*}";                                            \
		while true; do                                                 \
			touch "$$path/__init__.py" && chmod +x "$$path/__init__.py"; \
			if [[ $$path != *"/"* ]]; then break; fi;                    \
			path=$${path%/*};                                            \
		done                                                           \
	done

python_cpp_pkg_flags = `pkg-config --cflags --libs python`

lib_LTLIBRARIES = libbenchmark_messages.la
libbenchmark_messages_la_SOURCES = python/python_benchmark_messages.cc
libbenchmark_messages_la_LIBADD = $(top_srcdir)/src/.libs/libprotobuf.la
libbenchmark_messages_la_LDFLAGS = -version-info 1:0:0 -export-dynamic
libbenchmark_messages_la_CPPFLAGS = -I$(top_srcdir)/src -I$(srcdir)/cpp $(python_cpp_pkg_flags)
libbenchmark_messages_la-python_benchmark_messages.$(OBJEXT): $(benchmarks_protoc_outputs_header) $(benchmarks_protoc_outputs_proto2_header) $(benchmarks_protoc_outputs) $(benchmarks_protoc_outputs_proto2)
nodist_libbenchmark_messages_la_SOURCES =                         \
	$(benchmarks_protoc_outputs)                                    \
	$(benchmarks_protoc_outputs_proto2)                             \
	$(benchmarks_protoc_outputs_proto2_header)                      \
	$(benchmarks_protoc_outputs_header)

python-pure-python-benchmark: python_add_init
	@echo "Writing shortcut script python-pure-python-benchmark..."
	@echo '#! /bin/bash' > python-pure-python-benchmark
	@echo export LD_LIBRARY_PATH=$(top_srcdir)/src/.libs >> python-pure-python-benchmark
	@echo export DYLD_LIBRARY_PATH=$(top_srcdir)/src/.libs >> python-pure-python-benchmark
	@echo export PROTOCOL_BUFFERS_PYTHON_IMPLEMENTATION=\'python\' >> python-pure-python-benchmark
	@echo cp $(srcdir)/python/py_benchmark.py tmp >> python-pure-python-benchmark
	@echo python tmp/py_benchmark.py '$$@' >> python-pure-python-benchmark
	@chmod +x python-pure-python-benchmark

python-cpp-reflection-benchmark: python_add_init
	@echo "Writing shortcut script python-cpp-reflection-benchmark..."
	@echo '#! /bin/bash' > python-cpp-reflection-benchmark
	@echo export LD_LIBRARY_PATH=$(top_srcdir)/src/.libs >> python-cpp-reflection-benchmark
	@echo export DYLD_LIBRARY_PATH=$(top_srcdir)/src/.libs >> python-cpp-reflection-benchmark
	@echo export PROTOCOL_BUFFERS_PYTHON_IMPLEMENTATION=\'cpp\' >> python-cpp-reflection-benchmark
	@echo cp $(srcdir)/python/py_benchmark.py tmp >> python-cpp-reflection-benchmark
	@echo python tmp/py_benchmark.py '$$@' >> python-cpp-reflection-benchmark
	@chmod +x python-cpp-reflection-benchmark

python-cpp-generated-code-benchmark: python_add_init libbenchmark_messages.la
	@echo "Writing shortcut script python-cpp-generated-code-benchmark..."
	@echo '#! /bin/bash' > python-cpp-generated-code-benchmark
	@echo export LD_LIBRARY_PATH=$(top_srcdir)/src/.libs >> python-cpp-generated-code-benchmark
	@echo export DYLD_LIBRARY_PATH=$(top_srcdir)/src/.libs >> python-cpp-generated-code-benchmark
	@echo export PROTOCOL_BUFFERS_PYTHON_IMPLEMENTATION=\'cpp\' >> python-cpp-generated-code-benchmark
	@echo cp $(srcdir)/python/py_benchmark.py tmp >> python-cpp-generated-code-benchmark
	@echo python tmp/py_benchmark.py --cpp_generated '$$@' >> python-cpp-generated-code-benchmark
	@chmod +x python-cpp-generated-code-benchmark

python-pure-python: python-pure-python-benchmark
	./python-pure-python-benchmark $(all_data)

python-cpp-reflection: python-cpp-reflection-benchmark
	./python-cpp-reflection-benchmark $(all_data)

python-cpp-generated-code: python-cpp-generated-code-benchmark
	./python-cpp-generated-code-benchmark $(all_data)

############# PYTHON RULES END ##############

############# GO RULES BEGIN ##############

benchmarks_protoc_inputs_proto2_message1 =                                 \
	datasets/google_message1/proto2/benchmark_message1_proto2.proto

benchmarks_protoc_inputs_proto2_message2 =                                 \
	datasets/google_message2/benchmark_message2.proto

benchmarks_protoc_inputs_proto2_message3 =                                 \
	datasets/google_message3/benchmark_message3.proto                        \
	datasets/google_message3/benchmark_message3_1.proto                      \
	datasets/google_message3/benchmark_message3_2.proto                      \
	datasets/google_message3/benchmark_message3_3.proto                      \
	datasets/google_message3/benchmark_message3_4.proto                      \
	datasets/google_message3/benchmark_message3_5.proto                      \
	datasets/google_message3/benchmark_message3_6.proto                      \
	datasets/google_message3/benchmark_message3_7.proto                      \
	datasets/google_message3/benchmark_message3_8.proto

benchmarks_protoc_inputs_proto2_message4 =                                 \
	datasets/google_message4/benchmark_message4.proto                        \
	datasets/google_message4/benchmark_message4_1.proto                      \
	datasets/google_message4/benchmark_message4_2.proto                      \
	datasets/google_message4/benchmark_message4_3.proto

go_protoc_middleman: make_tmp_dir $(top_srcdir)/src/protoc$(EXEEXT) $(benchmarks_protoc_inputs) $(well_known_type_protoc_inputs) $(benchmarks_protoc_inputs_proto2_message1) $(benchmarks_protoc_inputs_proto2_message2) $(benchmarks_protoc_inputs_proto2_message3) $(benchmarks_protoc_inputs_proto2_message4) $(well_known_type_protoc_inputs)
	oldpwd=`pwd` && ( cd $(srcdir) && $$oldpwd/../src/protoc$(EXEEXT) -I. -I$(top_srcdir)/src --go_out=$$oldpwd/tmp $(benchmarks_protoc_inputs) )
	oldpwd=`pwd` && ( cd $(srcdir) && $$oldpwd/../src/protoc$(EXEEXT) -I. -I$(top_srcdir)/src --go_out=$$oldpwd/tmp $(benchmarks_protoc_inputs_benchmark_wrapper) )
	oldpwd=`pwd` && ( cd $(srcdir) && $$oldpwd/../src/protoc$(EXEEXT) -I. -I$(top_srcdir)/src --go_out=$$oldpwd/tmp $(benchmarks_protoc_inputs_proto2_message1) )
	oldpwd=`pwd` && ( cd $(srcdir) && $$oldpwd/../src/protoc$(EXEEXT) -I. -I$(top_srcdir)/src --go_out=$$oldpwd/tmp $(benchmarks_protoc_inputs_proto2_message2) )
	oldpwd=`pwd` && ( cd $(srcdir) && $$oldpwd/../src/protoc$(EXEEXT) -I. -I$(top_srcdir)/src --go_out=$$oldpwd/tmp $(benchmarks_protoc_inputs_proto2_message3) )
	oldpwd=`pwd` && ( cd $(srcdir) && $$oldpwd/../src/protoc$(EXEEXT) -I. -I$(top_srcdir)/src --go_out=$$oldpwd/tmp $(benchmarks_protoc_inputs_proto2_message4) )
	touch go_protoc_middleman

go-benchmark: go_protoc_middleman
	@echo "Writing shortcut script go-benchmark..."
	@echo '#! /bin/bash' > go-benchmark
	@echo 'cd $(srcdir)/go' >> go-benchmark
	@echo 'all_data=""' >> go-benchmark
	@echo 'conf=()' >> go-benchmark
	@echo 'data_files=()' >> go-benchmark
	@echo 'for arg in $$@; do if [[ $${arg:0:1} == "-" ]]; then conf+=($$arg); else data_files+=("$$arg"); fi; done' >> go-benchmark
	@echo 'go test -bench=. $${conf[*]} -- $${data_files[*]}' >> go-benchmark
	@echo 'cd ..' >> go-benchmark
	@chmod +x go-benchmark

go: go_protoc_middleman go-benchmark
	./go-benchmark $(all_data)

############# GO RULES END ##############

############# GOGO RULES BEGIN ############

cpp_no_group_benchmarks_protoc_outputs_header =                             \
	gogo/cpp_no_group/benchmarks.pb.h                                         \
	gogo/cpp_no_group/datasets/google_message1/proto3/benchmark_message1_proto3.pb.h

cpp_no_group_benchmarks_protoc_outputs =                                    \
	gogo/cpp_no_group/benchmarks.pb.cc                                        \
	gogo/cpp_no_group/datasets/google_message1/proto3/benchmark_message1_proto3.pb.cc

cpp_no_group_benchmarks_protoc_outputs_proto2_header =                      \
	gogo/cpp_no_group/datasets/google_message1/proto2/benchmark_message1_proto2.pb.h \
	gogo/cpp_no_group/datasets/google_message2/benchmark_message2.pb.h        \
	gogo/cpp_no_group/datasets/google_message3/benchmark_message3.pb.h        \
	gogo/cpp_no_group/datasets/google_message3/benchmark_message3_1.pb.h      \
	gogo/cpp_no_group/datasets/google_message3/benchmark_message3_2.pb.h      \
	gogo/cpp_no_group/datasets/google_message3/benchmark_message3_3.pb.h      \
	gogo/cpp_no_group/datasets/google_message3/benchmark_message3_4.pb.h      \
	gogo/cpp_no_group/datasets/google_message3/benchmark_message3_5.pb.h      \
	gogo/cpp_no_group/datasets/google_message3/benchmark_message3_6.pb.h      \
	gogo/cpp_no_group/datasets/google_message3/benchmark_message3_7.pb.h      \
	gogo/cpp_no_group/datasets/google_message3/benchmark_message3_8.pb.h      \
	gogo/cpp_no_group/datasets/google_message4/benchmark_message4.pb.h        \
	gogo/cpp_no_group/datasets/google_message4/benchmark_message4_1.pb.h      \
	gogo/cpp_no_group/datasets/google_message4/benchmark_message4_2.pb.h      \
	gogo/cpp_no_group/datasets/google_message4/benchmark_message4_3.pb.h

cpp_no_group_benchmarks_protoc_outputs_proto2 =                             \
	gogo/cpp_no_group/datasets/google_message1/proto2/benchmark_message1_proto2.pb.cc \
	gogo/cpp_no_group/datasets/google_message2/benchmark_message2.pb.cc       \
	gogo/cpp_no_group/datasets/google_message3/benchmark_message3.pb.cc       \
	gogo/cpp_no_group/datasets/google_message3/benchmark_message3_1.pb.cc     \
	gogo/cpp_no_group/datasets/google_message3/benchmark_message3_2.pb.cc     \
	gogo/cpp_no_group/datasets/google_message3/benchmark_message3_3.pb.cc     \
	gogo/cpp_no_group/datasets/google_message3/benchmark_message3_4.pb.cc     \
	gogo/cpp_no_group/datasets/google_message3/benchmark_message3_5.pb.cc     \
	gogo/cpp_no_group/datasets/google_message3/benchmark_message3_6.pb.cc     \
	gogo/cpp_no_group/datasets/google_message3/benchmark_message3_7.pb.cc     \
	gogo/cpp_no_group/datasets/google_message3/benchmark_message3_8.pb.cc     \
	gogo/cpp_no_group/datasets/google_message4/benchmark_message4.pb.cc       \
	gogo/cpp_no_group/datasets/google_message4/benchmark_message4_1.pb.cc     \
	gogo/cpp_no_group/datasets/google_message4/benchmark_message4_2.pb.cc     \
	gogo/cpp_no_group/datasets/google_message4/benchmark_message4_3.pb.cc

$(cpp_no_group_benchmarks_protoc_outputs): cpp_no_group_protoc_middleman
$(cpp_no_group_benchmarks_protoc_outputs_header): cpp_no_group_protoc_middleman
$(cpp_no_group_benchmarks_protoc_outputs_proto2): cpp_no_group_protoc_middleman
$(cpp_no_group_benchmarks_protoc_outputs_proto2_header): cpp_no_group_protoc_middleman

generate_cpp_no_group_benchmark_code:
	cp $(srcdir)/cpp/cpp_benchmark.cc gogo/cpp_no_group/cpp_benchmark.cc
	sed -i -e "s/\#include \"datasets/\#include \"gogo\/cpp_no_group\/datasets/g" gogo/cpp_no_group/cpp_benchmark.cc
	sed -i -e "s/\#include \"benchmarks.pb.h/\#include \"gogo\/cpp_no_group\/benchmarks.pb.h/g" gogo/cpp_no_group/cpp_benchmark.cc
	touch generate_cpp_no_group_benchmark_code

bin_PROGRAMS += cpp-no-group-benchmark
cpp_no_group_benchmark_LDADD = $(top_srcdir)/src/libprotobuf.la $(top_srcdir)/third_party/benchmark/src/libbenchmark.a
cpp_no_group_benchmark_SOURCES = gogo/cpp_no_group/cpp_benchmark.cc
cpp_no_group_benchmark_CPPFLAGS = -I$(top_srcdir)/src -I$(srcdir)/gogo/cpp_no_group -I$(top_srcdir)/third_party/benchmark/include
# Explicit deps because BUILT_SOURCES are only done before a "make all/check"
# so a direct "make test_cpp" could fail if parallel enough.
# See: https://www.gnu.org/software/automake/manual/html_node/Built-Sources-Example.html#Recording-Dependencies-manually
gogo/cpp_no_group/cpp_no_group_benchmark-cpp_benchmark.$(OBJEXT): $(cpp_no_group_benchmarks_protoc_outputs) $(cpp_no_group_benchmarks_protoc_outputs_proto2) $(cpp_no_group_benchmarks_protoc_outputs_header) \
	$(cpp_no_group_benchmarks_protoc_outputs_proto2_header) $(top_srcdir)/third_party/benchmark/src/libbenchmark.a generate_cpp_no_group_benchmark_code
gogo/cpp_no_group/cpp_benchmark.cc: generate_cpp_no_group_benchmark_code
nodist_cpp_no_group_benchmark_SOURCES =                                    \
	$(cpp_no_group_benchmarks_protoc_outputs_proto2)                         \
	$(cpp_no_group_benchmarks_protoc_outputs)                                \
	$(cpp_no_group_benchmarks_protoc_outputs_header)                         \
	$(cpp_no_group_benchmarks_protoc_outputs_proto2_header)

cpp_no_group: cpp_no_group_protoc_middleman generate_gogo_data cpp-no-group-benchmark
	./cpp-no-group-benchmark $(gogo_data)

gogo_proto_middleman: protoc-gen-gogoproto
	mkdir -p "tmp/gogo_proto"
	oldpwd=`pwd` && ( cd $(srcdir) && $$oldpwd/../src/protoc$(EXEEXT) -I$(srcdir) -I$(top_srcdir) --plugin=protoc-gen-gogoproto --gogoproto_out=$$oldpwd/tmp/gogo_proto $(benchmarks_protoc_inputs) $(benchmarks_protoc_inputs_benchmark_wrapper) $(benchmarks_protoc_inputs_proto2) )
	touch gogo_proto_middleman

gogo_data = $$(for data in $(all_data); do echo "tmp/gogo_data$${data\#$(srcdir)}"; done | xargs)

generate_gogo_data: protoc_middleman protoc_middleman2 gogo-data-scrubber
	mkdir -p `dirname $(gogo_data)`
	./gogo-data-scrubber $(all_data) $(gogo_data)
	touch generate_gogo_data

make_tmp_dir_gogo:
	mkdir -p tmp/go_no_group/benchmark_code
	mkdir -p tmp/gogofast/benchmark_code
	mkdir -p tmp/gogofaster/benchmark_code
	mkdir -p tmp/gogoslick/benchmark_code
	touch make_tmp_dir_gogo

go_no_group_protoc_middleman: make_tmp_dir_gogo $(top_srcdir)/src/protoc$(EXEEXT) gogo_proto_middleman $(well_known_type_protoc_inputs)
	oldpwd=`pwd` && ( cd $(srcdir)/tmp/gogo_proto && $$oldpwd/../src/protoc$(EXEEXT) -I. -I$$oldpwd/$(top_srcdir)/src --go_out=$$oldpwd/tmp/go_no_group $(benchmarks_protoc_inputs) )
	oldpwd=`pwd` && ( cd $(srcdir)/tmp/gogo_proto && $$oldpwd/../src/protoc$(EXEEXT) -I. -I$$oldpwd/$(top_srcdir)/src --go_out=$$oldpwd/tmp/go_no_group $(benchmarks_protoc_inputs_benchmark_wrapper) )
	oldpwd=`pwd` && ( cd $(srcdir)/tmp/gogo_proto && $$oldpwd/../src/protoc$(EXEEXT) -I. -I$$oldpwd/$(top_srcdir)/src --go_out=$$oldpwd/tmp/go_no_group $(benchmarks_protoc_inputs_proto2_message1) )
	oldpwd=`pwd` && ( cd $(srcdir)/tmp/gogo_proto && $$oldpwd/../src/protoc$(EXEEXT) -I. -I$$oldpwd/$(top_srcdir)/src --go_out=$$oldpwd/tmp/go_no_group $(benchmarks_protoc_inputs_proto2_message2) )
	oldpwd=`pwd` && ( cd $(srcdir)/tmp/gogo_proto && $$oldpwd/../src/protoc$(EXEEXT) -I. -I$$oldpwd/$(top_srcdir)/src --go_out=$$oldpwd/tmp/go_no_group $(benchmarks_protoc_inputs_proto2_message3) )
	oldpwd=`pwd` && ( cd $(srcdir)/tmp/gogo_proto && $$oldpwd/../src/protoc$(EXEEXT) -I. -I$$oldpwd/$(top_srcdir)/src --go_out=$$oldpwd/tmp/go_no_group $(benchmarks_protoc_inputs_proto2_message4) )
	touch go_no_group_protoc_middleman

cpp_no_group_protoc_middleman: make_tmp_dir_gogo $(top_srcdir)/src/protoc$(EXEEXT) gogo_proto_middleman $(well_known_type_protoc_inputs)
	oldpwd=`pwd` && ( cd $(srcdir)/tmp/gogo_proto && $$oldpwd/../src/protoc$(EXEEXT) -I. -I$$oldpwd/$(top_srcdir)/src --cpp_out=$$oldpwd/gogo/cpp_no_group $(benchmarks_protoc_inputs) )
	oldpwd=`pwd` && ( cd $(srcdir)/tmp/gogo_proto && $$oldpwd/../src/protoc$(EXEEXT) -I. -I$$oldpwd/$(top_srcdir)/src --cpp_out=$$oldpwd/gogo/cpp_no_group $(benchmarks_protoc_inputs_benchmark_wrapper) )
	oldpwd=`pwd` && ( cd $(srcdir)/tmp/gogo_proto && $$oldpwd/../src/protoc$(EXEEXT) -I. -I$$oldpwd/$(top_srcdir)/src --cpp_out=$$oldpwd/gogo/cpp_no_group $(benchmarks_protoc_inputs_proto2_message1) )
	oldpwd=`pwd` && ( cd $(srcdir)/tmp/gogo_proto && $$oldpwd/../src/protoc$(EXEEXT) -I. -I$$oldpwd/$(top_srcdir)/src --cpp_out=$$oldpwd/gogo/cpp_no_group $(benchmarks_protoc_inputs_proto2_message2) )
	oldpwd=`pwd` && ( cd $(srcdir)/tmp/gogo_proto && $$oldpwd/../src/protoc$(EXEEXT) -I. -I$$oldpwd/$(top_srcdir)/src --cpp_out=$$oldpwd/gogo/cpp_no_group $(benchmarks_protoc_inputs_proto2_message3) )
	oldpwd=`pwd` && ( cd $(srcdir)/tmp/gogo_proto && $$oldpwd/../src/protoc$(EXEEXT) -I. -I$$oldpwd/$(top_srcdir)/src --cpp_out=$$oldpwd/gogo/cpp_no_group $(benchmarks_protoc_inputs_proto2_message4) )
	touch cpp_no_group_protoc_middleman

gogofast_protoc_middleman: make_tmp_dir_gogo $(top_srcdir)/src/protoc$(EXEEXT) gogo_proto_middleman $(well_known_type_protoc_inputs)
	oldpwd=`pwd` && ( cd $(srcdir)/tmp/gogo_proto && $$oldpwd/../src/protoc$(EXEEXT) -I. -I$$oldpwd/$(top_srcdir)/src --gogofast_out=$$oldpwd/tmp/gogofast $(benchmarks_protoc_inputs) )
	oldpwd=`pwd` && ( cd $(srcdir)/tmp/gogo_proto && $$oldpwd/../src/protoc$(EXEEXT) -I. -I$$oldpwd/$(top_srcdir)/src --gogofast_out=$$oldpwd/tmp/gogofast $(benchmarks_protoc_inputs_benchmark_wrapper) )
	oldpwd=`pwd` && ( cd $(srcdir)/tmp/gogo_proto && $$oldpwd/../src/protoc$(EXEEXT) -I. -I$$oldpwd/$(top_srcdir)/src --gogofast_out=$$oldpwd/tmp/gogofast $(benchmarks_protoc_inputs_proto2_message1) )
	oldpwd=`pwd` && ( cd $(srcdir)/tmp/gogo_proto && $$oldpwd/../src/protoc$(EXEEXT) -I. -I$$oldpwd/$(top_srcdir)/src --gogofast_out=$$oldpwd/tmp/gogofast $(benchmarks_protoc_inputs_proto2_message2) )
	oldpwd=`pwd` && ( cd $(srcdir)/tmp/gogo_proto && $$oldpwd/../src/protoc$(EXEEXT) -I. -I$$oldpwd/$(top_srcdir)/src --gogofast_out=$$oldpwd/tmp/gogofast $(benchmarks_protoc_inputs_proto2_message3) )
	oldpwd=`pwd` && ( cd $(srcdir)/tmp/gogo_proto && $$oldpwd/../src/protoc$(EXEEXT) -I. -I$$oldpwd/$(top_srcdir)/src --gogofast_out=$$oldpwd/tmp/gogofast $(benchmarks_protoc_inputs_proto2_message4) )
	touch gogofast_protoc_middleman

gogofaster_protoc_middleman: make_tmp_dir_gogo $(top_srcdir)/src/protoc$(EXEEXT) gogo_proto_middleman $(well_known_type_protoc_inputs)
	oldpwd=`pwd` && ( cd $(srcdir)/tmp/gogo_proto && $$oldpwd/../src/protoc$(EXEEXT) -I. -I$$oldpwd/$(top_srcdir)/src --gogofaster_out=$$oldpwd/tmp/gogofaster $(benchmarks_protoc_inputs) )
	oldpwd=`pwd` && ( cd $(srcdir)/tmp/gogo_proto && $$oldpwd/../src/protoc$(EXEEXT) -I. -I$$oldpwd/$(top_srcdir)/src --gogofaster_out=$$oldpwd/tmp/gogofaster $(benchmarks_protoc_inputs_benchmark_wrapper) )
	oldpwd=`pwd` && ( cd $(srcdir)/tmp/gogo_proto && $$oldpwd/../src/protoc$(EXEEXT) -I. -I$$oldpwd/$(top_srcdir)/src --gogofaster_out=$$oldpwd/tmp/gogofaster $(benchmarks_protoc_inputs_proto2_message1) )
	oldpwd=`pwd` && ( cd $(srcdir)/tmp/gogo_proto && $$oldpwd/../src/protoc$(EXEEXT) -I. -I$$oldpwd/$(top_srcdir)/src --gogofaster_out=$$oldpwd/tmp/gogofaster $(benchmarks_protoc_inputs_proto2_message2) )
	oldpwd=`pwd` && ( cd $(srcdir)/tmp/gogo_proto && $$oldpwd/../src/protoc$(EXEEXT) -I. -I$$oldpwd/$(top_srcdir)/src --gogofaster_out=$$oldpwd/tmp/gogofaster $(benchmarks_protoc_inputs_proto2_message3) )
	oldpwd=`pwd` && ( cd $(srcdir)/tmp/gogo_proto && $$oldpwd/../src/protoc$(EXEEXT) -I. -I$$oldpwd/$(top_srcdir)/src --gogofaster_out=$$oldpwd/tmp/gogofaster $(benchmarks_protoc_inputs_proto2_message4) )
	touch gogofaster_protoc_middleman

gogoslick_protoc_middleman: make_tmp_dir_gogo $(top_srcdir)/src/protoc$(EXEEXT) gogo_proto_middleman $(well_known_type_protoc_inputs)
	oldpwd=`pwd` && ( cd $(srcdir)/tmp/gogo_proto && $$oldpwd/../src/protoc$(EXEEXT) -I. -I$$oldpwd/$(top_srcdir)/src --gogoslick_out=$$oldpwd/tmp/gogoslick $(benchmarks_protoc_inputs) )
	oldpwd=`pwd` && ( cd $(srcdir)/tmp/gogo_proto && $$oldpwd/../src/protoc$(EXEEXT) -I. -I$$oldpwd/$(top_srcdir)/src --gogoslick_out=$$oldpwd/tmp/gogoslick $(benchmarks_protoc_inputs_benchmark_wrapper) )
	oldpwd=`pwd` && ( cd $(srcdir)/tmp/gogo_proto && $$oldpwd/../src/protoc$(EXEEXT) -I. -I$$oldpwd/$(top_srcdir)/src --gogoslick_out=$$oldpwd/tmp/gogoslick $(benchmarks_protoc_inputs_proto2_message1) )
	oldpwd=`pwd` && ( cd $(srcdir)/tmp/gogo_proto && $$oldpwd/../src/protoc$(EXEEXT) -I. -I$$oldpwd/$(top_srcdir)/src --gogoslick_out=$$oldpwd/tmp/gogoslick $(benchmarks_protoc_inputs_proto2_message2) )
	oldpwd=`pwd` && ( cd $(srcdir)/tmp/gogo_proto && $$oldpwd/../src/protoc$(EXEEXT) -I. -I$$oldpwd/$(top_srcdir)/src --gogoslick_out=$$oldpwd/tmp/gogoslick $(benchmarks_protoc_inputs_proto2_message3) )
	oldpwd=`pwd` && ( cd $(srcdir)/tmp/gogo_proto && $$oldpwd/../src/protoc$(EXEEXT) -I. -I$$oldpwd/$(top_srcdir)/src --gogoslick_out=$$oldpwd/tmp/gogoslick $(benchmarks_protoc_inputs_proto2_message4) )
	touch gogoslick_protoc_middleman

generate-gogo-benchmark-code:
	@echo '#! /bin/bash' > generate-gogo-benchmark-code
	@echo 'cp $(srcdir)/go/go_benchmark_test.go tmp/$$1/benchmark_code/$$1_benchmark1_test.go' >> generate-gogo-benchmark-code
	@echo 'sed -i -e "s/\.\.\/tmp/../g" tmp/$$1/benchmark_code/$$1_benchmark1_test.go' >> generate-gogo-benchmark-code
	@echo 'sed -i -e "s/b\.Run(\"\(.*\)\"/b.Run(\"\1\_$$1\"/g" tmp/$$1/benchmark_code/$$1_benchmark1_test.go' >> generate-gogo-benchmark-code
	@echo 'if [[ $$2 == 1 ]]; then sed -i -e "s/github\.com\/golang/github.com\/gogo/g" tmp/$$1/benchmark_code/$$1_benchmark1_test.go; fi ' >> generate-gogo-benchmark-code
	@chmod +x generate-gogo-benchmark-code

generate_all_gogo_benchmark_code: generate-gogo-benchmark-code make_tmp_dir_gogo
	./generate-gogo-benchmark-code go_no_group 0
	./generate-gogo-benchmark-code gogofast 1
	./generate-gogo-benchmark-code gogofaster 1
	./generate-gogo-benchmark-code gogoslick 1

gogo-benchmark:
	@echo "Writing shortcut script gogo-benchmark..."
	@echo '#! /bin/bash' > gogo-benchmark
	@echo 'cd tmp/$$1/benchmark_code' >> gogo-benchmark
	@echo 'shift' >> gogo-benchmark
	@echo 'all_data=""' >> gogo-benchmark
	@echo 'for data_file in $$@; do all_data="$$all_data ../../../$$data_file"; done' >> gogo-benchmark
	@echo 'go test -bench=. -- $$all_data' >> gogo-benchmark
	@echo 'cd ../..' >> gogo-benchmark
	@chmod +x gogo-benchmark

go_no_group: go_no_group_protoc_middleman generate_gogo_data generate_all_gogo_benchmark_code gogo-benchmark
	./gogo-benchmark go_no_group $(gogo_data)

gogofast: gogofast_protoc_middleman generate_gogo_data gogo-benchmark generate_all_gogo_benchmark_code
	./gogo-benchmark gogofast $(gogo_data)

gogofaster: gogofaster_protoc_middleman  generate_gogo_data gogo-benchmark generate_all_gogo_benchmark_code
	./gogo-benchmark gogofaster $(gogo_data)

gogoslick: gogoslick_protoc_middleman  generate_gogo_data gogo-benchmark generate_all_gogo_benchmark_code
	./gogo-benchmark gogoslick $(gogo_data)


############# GOGO RULES END ############


############ UTIL RULES BEGIN ############

bin_PROGRAMS += protoc-gen-gogoproto gogo-data-scrubber protoc-gen-proto2_to_proto3 proto3-data-stripper

protoc_gen_gogoproto_LDADD = $(top_srcdir)/src/libprotobuf.la $(top_srcdir)/src/libprotoc.la
protoc_gen_gogoproto_SOURCES = util/protoc-gen-gogoproto.cc
protoc_gen_gogoproto_CPPFLAGS = -I$(top_srcdir)/src -I$(srcdir)/cpp -I$(srcdir)/util

gogo_data_scrubber_LDADD = $(top_srcdir)/src/libprotobuf.la
gogo_data_scrubber_SOURCES = util/gogo_data_scrubber.cc
gogo_data_scrubber_CPPFLAGS = -I$(top_srcdir)/src -I$(srcdir)/cpp -I$(srcdir)/util
util/gogo_data_scrubber-gogo_data_scrubber.$(OBJEXT): $(benchmarks_protoc_outputs) $(benchmarks_protoc_outputs_proto2) $(benchmarks_protoc_outputs_header) $(benchmarks_protoc_outputs_proto2_header)
nodist_gogo_data_scrubber_SOURCES =                                        \
	$(benchmarks_protoc_outputs)                                             \
	$(benchmarks_protoc_outputs_proto2)                                      \
	$(benchmarks_protoc_outputs_proto2_header)                               \
	$(benchmarks_protoc_outputs_header)

protoc_gen_proto2_to_proto3_LDADD = $(top_srcdir)/src/libprotobuf.la $(top_srcdir)/src/libprotoc.la
protoc_gen_proto2_to_proto3_SOURCES = util/protoc-gen-proto2_to_proto3.cc
protoc_gen_proto2_to_proto3_CPPFLAGS = -I$(top_srcdir)/src -I$(srcdir)/cpp -I$(srcdir)/util

proto3_data_stripper_LDADD = $(top_srcdir)/src/libprotobuf.la
proto3_data_stripper_SOURCES = util/proto3_data_stripper.cc
proto3_data_stripper_CPPFLAGS = -I$(top_srcdir)/src -I$(srcdir)/cpp -I$(srcdir)/util
util/proto3_data_stripper-proto3_data_stripper.$(OBJEXT): $(benchmarks_protoc_outputs) $(benchmarks_protoc_outputs_proto2) $(benchmarks_protoc_outputs_header) $(benchmarks_protoc_outputs_proto2_header)
nodist_proto3_data_stripper_SOURCES =                                      \
	$(benchmarks_protoc_outputs)                                             \
	$(benchmarks_protoc_outputs_proto2)                                      \
	$(benchmarks_protoc_outputs_proto2_header)                               \
	$(benchmarks_protoc_outputs_header)


############ UTIL RULES END ############

############ PROTO3 PREPARATION BEGIN #############

proto3_proto_middleman: protoc-gen-proto2_to_proto3
	mkdir -p "tmp/proto3_proto"
	oldpwd=`pwd` && ( cd $(srcdir) && $$oldpwd/../src/protoc$(EXEEXT) -I$(srcdir) -I$(top_srcdir) --plugin=protoc-gen-proto2_to_proto3 --proto2_to_proto3_out=$$oldpwd/tmp/proto3_proto $(benchmarks_protoc_inputs) $(benchmarks_protoc_inputs_benchmark_wrapper) $(benchmarks_protoc_inputs_proto2) )
	touch proto3_proto_middleman

full_srcdir = $$(cd $(srcdir) && pwd)
proto3_data = $$(for data in $(all_data); do echo $(full_srcdir)"/tmp/proto3_data$${data\#$(full_srcdir)}"; done | xargs)

generate_proto3_data: protoc_middleman protoc_middleman2 proto3-data-stripper
	mkdir -p `dirname $(proto3_data)`
	./proto3-data-stripper $(all_data) $(proto3_data)
	touch generate_proto3_data

############ PROTO3 PREPARATION END #############

############ PHP RULES BEGIN #################

proto3_middleman_php: proto3_proto_middleman
	mkdir -p "tmp/php"
	oldpwd=`pwd` && ( cd tmp/proto3_proto && $$oldpwd/../src/protoc$(EXEEXT) -I$(srcdir) -I$(top_srcdir) --php_out=$$oldpwd/tmp/php $(benchmarks_protoc_inputs) $(benchmarks_protoc_inputs_benchmark_wrapper) $(benchmarks_protoc_inputs_proto2) )
	touch proto3_middleman_php

php-benchmark: proto3_middleman_php generate_proto3_data
	mkdir -p "tmp/php/Google/Protobuf/Benchmark" && cp php/PhpBenchmark.php "tmp/php/Google/Protobuf/Benchmark"
	cp php/autoload.php "tmp/php"
	@echo "Writing shortcut script php-benchmark..."
	@echo '#! /bin/bash' > php-benchmark
	@echo 'export PROTOBUF_PHP_SRCDIR="$$(cd $(top_srcdir) && pwd)/php/src"' >> php-benchmark
	@echo 'cd tmp/php' >> php-benchmark
	@echo 'export CURRENT_DIR=$$(pwd)' >> php-benchmark
	@echo 'php -d auto_prepend_file="autoload.php" -d include_path="$$(pwd)" Google/Protobuf/Benchmark/PhpBenchmark.php $$@' >> php-benchmark
	@echo 'cd ../..' >> php-benchmark
	@chmod +x php-benchmark

php: php-benchmark proto3_middleman_php
	./php-benchmark --behavior_prefix="php" $(proto3_data)

php_c_extension:
	cd $(top_srcdir)/php/ext/google/protobuf && phpize && ./configure CFLAGS='-O3' && make -j8

php-c-benchmark: proto3_middleman_php generate_proto3_data php_c_extension php_c_extension
	mkdir -p "tmp/php/Google/Protobuf/Benchmark" && cp php/PhpBenchmark.php "tmp/php/Google/Protobuf/Benchmark"
	cp php/autoload.php "tmp/php"
	@echo "Writing shortcut script php-c-benchmark..."
	@echo '#! /bin/bash' > php-c-benchmark
	@echo 'export PROTOBUF_PHP_SRCDIR="$$(cd $(top_srcdir) && pwd)/php/src"' >> php-c-benchmark
	@echo 'export PROTOBUF_PHP_EXTDIR="$$PROTOBUF_PHP_SRCDIR/../ext/google/protobuf/modules"' >> php-c-benchmark
	@echo 'cd tmp/php' >> php-c-benchmark
	@echo 'export CURRENT_DIR=$$(pwd)' >> php-c-benchmark
	@echo 'php -d auto_prepend_file="autoload.php" -d include_path="$$(pwd)" -d extension="$$PROTOBUF_PHP_EXTDIR/protobuf.so" Google/Protobuf/Benchmark/PhpBenchmark.php $$@' >> php-c-benchmark
	@echo 'cd ../..' >> php-c-benchmark
	@chmod +x php-c-benchmark

php_c: php-c-benchmark proto3_middleman_php
	./php-c-benchmark --behavior_prefix="php_c" $(proto3_data)


############ PHP RULES END #################

############ protobuf.js RULE BEGIN #############

pbjs_preparation:
	mkdir -p tmp/protobuf.js
	cd tmp/protobuf.js && git clone https://github.com/dcodeIO/protobuf.js.git && \
			cd protobuf.js && npm install && npm run build
	cd tmp/protobuf.js && npm install benchmark
	cp protobuf.js/* tmp/protobuf.js
	cp js/benchmark_suite.js tmp/protobuf.js
	touch pbjs_preparation

pbjs_middleman: pbjs_preparation
	export OLDDIR=$$(pwd) && cd tmp/protobuf.js && node generate_pbjs_files.js --target static-module --include_path=$$OLDDIR -o generated_bundle_code.js $(benchmarks_protoc_inputs) $(benchmarks_protoc_inputs_benchmark_wrapper) $(benchmarks_protoc_inputs_proto2)
	touch pbjs_middleman

pbjs-benchmark: pbjs_middleman
	@echo '#! /bin/bash' > pbjs-benchmark
	@echo 'cd tmp/protobuf.js' >> pbjs-benchmark
	@echo 'sed -i "s/protobufjs/.\/protobuf.js/g" generated_bundle_code.js' >> pbjs-benchmark
	@echo 'env NODE_PATH=".:./node_modules:$$NODE_PATH" node protobufjs_benchmark.js $$@' >> pbjs-benchmark
	@chmod +x pbjs-benchmark

pbjs: pbjs-benchmark
	./pbjs-benchmark $(all_data)

############ protobuf.js RULE END #############

############ JS RULE BEGIN #############

js_preparation:
	mkdir -p tmp/js
	oldpwd=$$(pwd) && cd $(top_srcdir)/js && npm install && npm test
	cd tmp/js && npm install benchmark
	cp js/* tmp/js
	touch js_preparation

js_middleman: js_preparation
	oldpwd=`pwd` && ( cd $(srcdir) && $$oldpwd/../src/protoc$(EXEEXT) -I. -I$(top_srcdir)/src --js_out=import_style=commonjs,binary:$$oldpwd/tmp/js $(benchmarks_protoc_inputs) $(benchmarks_protoc_inputs_benchmark_wrapper) $(benchmarks_protoc_inputs_proto2))
	touch js_middleman

js-benchmark: js_middleman
	@echo '#! /bin/bash' > js-benchmark
	@echo 'export TOP_JS_SRCDIR=$$(cd $(top_srcdir)/js && pwd)' >> js-benchmark
	@echo 'cd tmp/js' >> js-benchmark
	@echo 'env NODE_PATH="$$TOP_JS_SRCDIR:.:./node_modules:$$NODE_PATH" node --max-old-space-size=4096 js_benchmark.js $$@' >> js-benchmark
	@chmod +x js-benchmark

js: js-benchmark
	./js-benchmark $(all_data)

############ JS RULE END #############

MAINTAINERCLEANFILES =                                                     \
	Makefile.in

CLEANFILES =                                                               \
	$(benchmarks_protoc_outputs)                                             \
	$(benchmarks_protoc_outputs_header)                                      \
	$(benchmarks_protoc_outputs_proto2)                                      \
	$(benchmarks_protoc_outputs_proto2_header)                               \
	initialize_submodule                                                     \
	make_tmp_dir                                                             \
	protoc_middleman                                                         \
	protoc_middleman2                                                        \
	javac_middleman                                                          \
	java-benchmark                                                           \
	python_cpp_proto_library                                                 \
	python-pure-python-benchmark                                             \
	python-cpp-reflection-benchmark                                          \
	python-cpp-generated-code-benchmark                                      \
	go-benchmark                                                             \
	go_protoc_middleman                                                      \
	make_tmp_dir_gogo                                                        \
	gogo_proto_middleman                                                     \
	generate_gogo_data                                                       \
	go_no_group_protoc_middleman                                             \
	go_no_group                                                              \
	go-no-group-benchmark                                                    \
	$(cpp_no_group_benchmarks_protoc_outputs_header)                         \
	$(cpp_no_group_benchmarks_protoc_outputs)                                \
	$(cpp_no_group_benchmarks_protoc_outputs_proto2_header)                  \
	$(cpp_no_group_benchmarks_protoc_outputs_proto2)                         \
	generate_all_gogo_benchmark_code                                         \
	generate-gogo-benchmark-code                                             \
	cpp_no_group_protoc_middleman                                            \
	generate_cpp_no_group_benchmark_code                                     \
	generate_gogo_benchmark_code                                             \
	gogofast_protoc_middleman                                                \
	gogofast                                                                 \
	gogofaster_protoc_middleman                                              \
	gogofaster                                                               \
	gogoslick_protoc_middleman                                               \
	gogoslick                                                                \
	gogo-benchmark                                                           \
	gogo/cpp_no_group/cpp_benchmark.*                                        \
	proto3_proto_middleman                                                   \
	generate_proto3_data                                                     \
	php-benchmark                                                            \
	php-c-benchmark                                                          \
	proto3_middleman_php                                                     \
	pbjs_preparation                                                         \
	pbjs_middleman                                                           \
	pbjs-benchmark                                                           \
	js_preparation                                                           \
	js_middleman                                                             \
	js-benchmark

clean-local:
	-rm -rf tmp/*

