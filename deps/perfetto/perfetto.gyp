{
  'targets': [
    {
      'target_name': 'libperfetto',
      'type': 'static_library',
      'cflags': ['-fvisibility=hidden'],
      'xcode_settings': {
        'GCC_SYMBOLS_PRIVATE_EXTERN': 'YES',  # -fvisibility=hidden
      },
      'dependencies': [
        '../protobuf/protobuf.gyp:protobuf-lite',
        '../protobuf/protobuf.gyp:protoc',
        ':ipc_plugin',
        ':protozero_plugin',
        ':cppgen_plugin',
        ':protoc-gen',
        '../zlib/zlib.gyp:zlib',
        'jsoncpp.gyp:jsoncpp',
      ],
      'include_dirs': [
        'config',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/protozero',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/ipc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cppgen',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp',
        'perfetto/include',
        'perfetto/src',
        'perfetto',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'config',
          '<@(SHARED_INTERMEDIATE_DIR)/perfetto/protozero',
          '<@(SHARED_INTERMEDIATE_DIR)/perfetto/ipc',
          '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cppgen',
          '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp',
          'perfetto/include',
          'perfetto/src',
          'perfetto',
        ]
      },
      'defines': [
        'GOOGLE_PROTOBUF_NO_RTTI',
        'GOOGLE_PROTOBUF_NO_STATIC_INITIALIZER',
      ],
      'sources': [

        # trace_processor:export_json
        'perfetto/src/trace_processor/export_json.cc',

        # trace_processor/types
        'perfetto/src/trace_processor/types/destructible.cc',
        'perfetto/src/trace_processor/types/gfp_flags.cc',
        'perfetto/src/trace_processor/types/task_state.cc',
        'perfetto/src/trace_processor/types/variadic.cc',

        # trace_processor/storage
        'perfetto/src/trace_processor/storage/trace_storage.cc',

        # trace_processor/containers
        'perfetto/src/trace_processor/containers/bit_vector.cc',
        'perfetto/src/trace_processor/containers/bit_vector_iterators.cc',
        'perfetto/src/trace_processor/containers/row_map.cc',
        'perfetto/src/trace_processor/containers/sparse_vector.cc',
        'perfetto/src/trace_processor/containers/string_pool.cc',

        # trace_processor/tables
        'perfetto/src/trace_processor/tables/table_destructors.cc',

        # trace_processor/db
        'perfetto/src/trace_processor/db/column.cc',
        'perfetto/src/trace_processor/db/table.cc',

        # trace_processor:storage_minimal
        'perfetto/src/trace_processor/forwarding_trace_parser.cc',
        'perfetto/src/trace_processor/importers/default_modules.cc',
        'perfetto/src/trace_processor/importers/ftrace/ftrace_module.cc',
        'perfetto/src/trace_processor/importers/gzip/gzip_trace_parser.cc',
        'perfetto/src/trace_processor/importers/gzip/gzip_utils.cc',
        'perfetto/src/trace_processor/importers/json/json_utils.cc',
        'perfetto/src/trace_processor/importers/json/json_trace_parser.cc',
        'perfetto/src/trace_processor/importers/json/json_trace_tokenizer.cc',
        'perfetto/src/trace_processor/importers/json/json_tracker.cc',
        'perfetto/src/trace_processor/importers/ninja/ninja_log_parser.cc',
        'perfetto/src/trace_processor/importers/proto/args_table_utils.cc',
        'perfetto/src/trace_processor/importers/proto/heap_profile_tracker.cc',
        'perfetto/src/trace_processor/importers/proto/metadata_tracker.cc',
        'perfetto/src/trace_processor/importers/proto/packet_sequence_state.cc',
        'perfetto/src/trace_processor/importers/proto/perf_sample_tracker.cc',
        'perfetto/src/trace_processor/importers/proto/profile_module.cc',
        'perfetto/src/trace_processor/importers/proto/profile_packet_utils.cc',
        'perfetto/src/trace_processor/importers/proto/proto_importer_module.cc',
        'perfetto/src/trace_processor/importers/proto/proto_trace_parser.cc',
        'perfetto/src/trace_processor/importers/proto/proto_trace_tokenizer.cc',
        'perfetto/src/trace_processor/importers/proto/stack_profile_tracker.cc',
        'perfetto/src/trace_processor/importers/proto/track_event_module.cc',
        'perfetto/src/trace_processor/importers/proto/track_event_parser.cc',
        'perfetto/src/trace_processor/importers/proto/track_event_tokenizer.cc',
        'perfetto/src/trace_processor/trace_processor_context.cc',
        'perfetto/src/trace_processor/trace_processor_storage.cc',
        'perfetto/src/trace_processor/trace_processor_storage_impl.cc',
        'perfetto/src/trace_processor/trace_sorter.cc',
        'perfetto/src/trace_processor/virtual_destructors.cc',

        # trace_processor/util
        'perfetto/src/trace_processor/util/descriptors.cc',
        'perfetto/src/trace_processor/util/proto_to_json.cc',

        # trace_processor/importers:common
        'perfetto/src/trace_processor/importers/common/args_tracker.cc',
        'perfetto/src/trace_processor/importers/common/clock_tracker.cc',
        'perfetto/src/trace_processor/importers/common/event_tracker.cc',
        'perfetto/src/trace_processor/importers/common/global_args_tracker.cc',
        'perfetto/src/trace_processor/importers/common/process_tracker.cc',
        'perfetto/src/trace_processor/importers/common/slice_tracker.cc',
        'perfetto/src/trace_processor/importers/common/track_tracker.cc',

        # tracing:platform_fake
        #'perfetto/src/tracing/platform_fake.cc',

        # tracing:platform_posix
        'perfetto/src/tracing/platform_posix.cc',

        # tracing:in_process_backend
        'perfetto/src/tracing/internal/in_process_tracing_backend.cc',

        # tracing:system_backend
        'perfetto/src/tracing/internal/system_tracing_backend.cc',

        # tracing:client_api_without_backends
        'perfetto/src/tracing/data_source.cc',
        'perfetto/src/tracing/debug_annotation.cc',
        'perfetto/src/tracing/event_context.cc',
        'perfetto/src/tracing/internal/tracing_muxer_impl.cc',
        'perfetto/src/tracing/internal/tracing_muxer_impl.h',
        'perfetto/src/tracing/internal/track_event_internal.cc',
        'perfetto/src/tracing/platform.cc',
        'perfetto/src/tracing/tracing.cc',
        'perfetto/src/tracing/track.cc',
        'perfetto/src/tracing/track_event_category_registry.cc',
        'perfetto/src/tracing/track_event_legacy.cc',
        'perfetto/src/tracing/virtual_destructors.cc',

        # tracing/core:service
        'perfetto/src/tracing/core/metatrace_writer.cc',
        'perfetto/src/tracing/core/packet_stream_validator.cc',
        'perfetto/src/tracing/core/trace_buffer.cc',
        'perfetto/src/tracing/core/tracing_service_impl.cc',

        # tracing/core:core
        'perfetto/src/tracing/core/id_allocator.cc',
        'perfetto/src/tracing/core/id_allocator.h',
        'perfetto/src/tracing/core/null_trace_writer.cc',
        'perfetto/src/tracing/core/null_trace_writer.h',
        'perfetto/src/tracing/core/patch_list.h',
        'perfetto/src/tracing/core/shared_memory_abi.cc',
        'perfetto/src/tracing/core/shared_memory_arbiter_impl.cc',
        'perfetto/src/tracing/core/shared_memory_arbiter_impl.h',
        'perfetto/src/tracing/core/trace_packet.cc',
        'perfetto/src/tracing/core/trace_writer_impl.cc',
        'perfetto/src/tracing/core/trace_writer_impl.h',
        'perfetto/src/tracing/core/virtual_destructors.cc',

        # tracing:common
        'perfetto/src/tracing/trace_writer_base.cc',

        # tracing/ipc/common
        'perfetto/src/tracing/ipc/default_socket.cc',
        'perfetto/src/tracing/ipc/memfd.cc',
        'perfetto/src/tracing/ipc/posix_shared_memory.cc',

        # tracing/core
        'perfetto/src/tracing/core/id_allocator.cc',
        'perfetto/src/tracing/core/null_trace_writer.cc',
        'perfetto/src/tracing/core/shared_memory_abi.cc',
        'perfetto/src/tracing/core/shared_memory_arbiter_impl.cc',
        'perfetto/src/tracing/core/trace_packet.cc',
        'perfetto/src/tracing/core/trace_writer_impl.cc',
        'perfetto/src/tracing/core/virtual_destructors.cc',

        # tracing/core:service
        'perfetto/src/tracing/core/metatrace_writer.cc',
        'perfetto/src/tracing/core/metatrace_writer.h',
        'perfetto/src/tracing/core/packet_stream_validator.cc',
        'perfetto/src/tracing/core/packet_stream_validator.h',
        'perfetto/src/tracing/core/trace_buffer.cc',
        'perfetto/src/tracing/core/trace_buffer.h',
        'perfetto/src/tracing/core/tracing_service_impl.cc',
        'perfetto/src/tracing/core/tracing_service_impl.h',

        # ipc:common
        'perfetto/src/ipc/buffered_frame_deserializer.cc',
        'perfetto/src/ipc/buffered_frame_deserializer.h',
        'perfetto/src/ipc/deferred.cc',
        'perfetto/src/ipc/virtual_destructors.cc',

        # ipc/consumer
        'perfetto/src/tracing/ipc/consumer/consumer_ipc_client_impl.cc',

        # ipc/producer
        'perfetto/src/tracing/ipc/producer/producer_ipc_client_impl.cc',

        # ipc/service
        'perfetto/src/tracing/ipc/service/consumer_ipc_service.cc',
        'perfetto/src/tracing/ipc/service/consumer_ipc_service.h',
        'perfetto/src/tracing/ipc/service/producer_ipc_service.cc',
        'perfetto/src/tracing/ipc/service/producer_ipc_service.h',
        'perfetto/src/tracing/ipc/service/service_ipc_host_impl.cc',
        'perfetto/src/tracing/ipc/service/service_ipc_host_impl.h',

        # ipc:client
        'perfetto/src/ipc/client_impl.cc',
        'perfetto/src/ipc/client_impl.h',
        'perfetto/src/ipc/service_proxy.cc',

        # ipc:host
        'perfetto/src/ipc/host_impl.cc',

        # protozero
        'perfetto/src/protozero/field.cc',
        'perfetto/src/protozero/message.cc',
        'perfetto/src/protozero/message_handle.cc',
        'perfetto/src/protozero/packed_repeated_fields.cc',
        'perfetto/src/protozero/proto_decoder.cc',
        'perfetto/src/protozero/scattered_heap_buffer.cc',
        'perfetto/src/protozero/scattered_stream_null_delegate.cc',
        'perfetto/src/protozero/scattered_stream_writer.cc',
        'perfetto/src/protozero/static_buffer.cc',
        'perfetto/src/protozero/virtual_destructors.cc',

        # base
        'perfetto/src/base/file_utils.cc',
        'perfetto/src/base/logging.cc',
        'perfetto/src/base/metatrace.cc',
        'perfetto/src/base/paged_memory.cc',
        'perfetto/src/base/string_splitter.cc',
        'perfetto/src/base/string_utils.cc',
        'perfetto/src/base/string_view.cc',
        'perfetto/src/base/subprocess.cc',
        'perfetto/src/base/thread_checker.cc',
        'perfetto/src/base/time.cc',
        'perfetto/src/base/uuid.cc',
        'perfetto/src/base/virtual_destructors.cc',
        'perfetto/src/base/waitable_event.cc',
        'perfetto/src/base/watchdog_posix.cc',
        'perfetto/src/base/unix_socket.cc',

        # base(posix)
        'perfetto/src/base/event_fd.cc',
        'perfetto/src/base/pipe.cc',
        'perfetto/src/base/temp_file.cc',
        'perfetto/src/base/thread_task_runner.cc',
        'perfetto/src/base/unix_task_runner.cc',

        # Everything below here is generated by tools/protoc.py
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cppgen/protos/perfetto/config/data_source_config.gen.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cppgen/protos/perfetto/config/profiling/java_hprof_config.gen.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cppgen/protos/perfetto/config/profiling/heapprofd_config.gen.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cppgen/protos/perfetto/config/profiling/perf_event_config.gen.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cppgen/protos/perfetto/config/trace_config.gen.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cppgen/protos/perfetto/config/sys_stats/sys_stats_config.gen.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cppgen/protos/perfetto/config/inode_file/inode_file_config.gen.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cppgen/protos/perfetto/config/process_stats/process_stats_config.gen.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cppgen/protos/perfetto/config/ftrace/ftrace_config.gen.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cppgen/protos/perfetto/config/chrome/chrome_config.gen.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cppgen/protos/perfetto/config/power/android_power_config.gen.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cppgen/protos/perfetto/config/test_config.gen.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cppgen/protos/perfetto/config/android/packages_list_config.gen.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cppgen/protos/perfetto/config/android/android_polled_state_config.gen.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cppgen/protos/perfetto/config/android/android_log_config.gen.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cppgen/protos/perfetto/config/track_event/track_event_config.gen.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cppgen/protos/perfetto/config/gpu/vulkan_memory_config.gen.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cppgen/protos/perfetto/config/gpu/gpu_counter_config.gen.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cppgen/protos/perfetto/ipc/wire_protocol.gen.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cppgen/protos/perfetto/ipc/consumer_port.gen.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cppgen/protos/perfetto/ipc/producer_port.gen.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cppgen/protos/perfetto/common/commit_data_request.gen.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cppgen/protos/perfetto/common/data_source_descriptor.gen.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cppgen/protos/perfetto/common/tracing_service_state.gen.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cppgen/protos/perfetto/common/trace_stats.gen.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cppgen/protos/perfetto/common/android_log_constants.gen.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cppgen/protos/perfetto/common/sys_stats_counters.gen.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cppgen/protos/perfetto/common/observable_events.gen.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cppgen/protos/perfetto/common/gpu_counter_descriptor.gen.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cppgen/protos/perfetto/common/descriptor.gen.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cppgen/protos/perfetto/common/tracing_service_capabilities.gen.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cppgen/protos/perfetto/common/track_event_descriptor.gen.cc',

        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/config/trace_config.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/config/profiling/perf_event_config.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/config/profiling/heapprofd_config.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/config/profiling/java_hprof_config.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/config/sys_stats/sys_stats_config.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/config/inode_file/inode_file_config.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/config/process_stats/process_stats_config.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/config/ftrace/ftrace_config.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/config/data_source_config.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/config/perfetto_config.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/config/chrome/chrome_config.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/config/power/android_power_config.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/config/android/android_log_config.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/config/android/packages_list_config.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/config/android/android_polled_state_config.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/config/track_event/track_event_config.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/config/gpu/gpu_counter_config.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/config/gpu/vulkan_memory_config.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/config/test_config.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/ipc/wire_protocol.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/ipc/consumer_port.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/ipc/producer_port.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/common/gpu_counter_descriptor.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/common/observable_events.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/common/data_source_descriptor.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/common/tracing_service_capabilities.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/common/descriptor.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/common/android_log_constants.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/common/track_event_descriptor.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/common/trace_stats.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/common/tracing_service_state.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/common/commit_data_request.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/common/sys_stats_counters.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/profiling/heap_graph.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/profiling/profile_packet.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/profiling/profile_common.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/profiling/smaps.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/trigger.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/trace_packet_defaults.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/sys_stats/sys_stats.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/trace_packet.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/test_event.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/perfetto_trace.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/ps/process_tree.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/ps/process_stats.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/system_info/cpu_info.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/ftrace/sde.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/ftrace/i2c.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/ftrace/oom.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/ftrace/regulator.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/ftrace/cgroup.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/ftrace/signal.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/ftrace/kmem.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/ftrace/mm_event.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/ftrace/clk.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/ftrace/ftrace_stats.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/ftrace/vmscan.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/ftrace/lowmemorykiller.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/ftrace/sched.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/ftrace/task.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/ftrace/f2fs.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/ftrace/mdss.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/ftrace/ftrace.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/ftrace/block.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/ftrace/ipi.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/ftrace/compaction.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/ftrace/workqueue.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/ftrace/ext4.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/ftrace/sync.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/ftrace/ftrace_event_bundle.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/ftrace/test_bundle_wrapper.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/ftrace/binder.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/ftrace/irq.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/ftrace/systrace.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/ftrace/power.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/ftrace/generic.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/ftrace/ion.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/ftrace/fence.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/ftrace/filemap.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/ftrace/ftrace_event.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/ftrace/raw_syscalls.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/interned_data/interned_data.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/chrome/chrome_trace_event.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/chrome/chrome_metadata.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/chrome/chrome_benchmark_metadata.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/chrome/chrome_trace_packet.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/trace.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/clock_snapshot.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/perfetto/tracing_service_event.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/perfetto/perfetto_metatrace.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/power/power_rails.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/power/battery_counters.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/filesystem/inode_file_map.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/android/graphics_frame_event.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/android/packages_list.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/android/initial_display_state.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/android/android_log.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/track_event/chrome_compositor_scheduler_state.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/track_event/debug_annotation.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/track_event/track_descriptor.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/track_event/counter_descriptor.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/track_event/chrome_user_event.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/track_event/chrome_process_descriptor.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/track_event/task_execution.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/track_event/track_event.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/track_event/chrome_legacy_ipc.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/track_event/thread_descriptor.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/track_event/source_location.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/track_event/chrome_keyed_service.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/track_event/log_message.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/track_event/process_descriptor.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/track_event/chrome_thread_descriptor.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/track_event/chrome_histogram_sample.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/track_event/chrome_latency_info.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/gpu/gpu_render_stage_event.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/gpu/gpu_log.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/gpu/vulkan_memory_event.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/gpu/vulkan_api_event.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/gpu/gpu_counter_event.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/system_info.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace_processor/metrics_impl.pb.cc',
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace_processor/trace_processor.pb.cc',

        # Node.js specific extension
        '<@(SHARED_INTERMEDIATE_DIR)/perfetto/cpp/protos/perfetto/trace/track_event/nodejs.pb.cc',
      ]
    },
    {
      'target_name': 'ipc_plugin',
      'type': 'executable',
      'dependencies': [
        '../protobuf/protobuf.gyp:libprotoc',
      ],
      'defines': [
        'GOOGLE_PROTOBUF_NO_RTTI',
        'GOOGLE_PROTOBUF_NO_STATIC_INITIALIZER',
      ],
      'include_dirs': [
        'config',
        'perfetto/include',
        'perfetto/src',
        'perfetto',
      ],
      'sources': [
        'perfetto/src/base/file_utils.cc',
        'perfetto/src/base/logging.cc',
        'perfetto/src/base/metatrace.cc',
        'perfetto/src/base/paged_memory.cc',
        'perfetto/src/base/string_splitter.cc',
        'perfetto/src/base/string_utils.cc',
        'perfetto/src/base/string_view.cc',
        'perfetto/src/base/subprocess.cc',
        'perfetto/src/base/thread_checker.cc',
        'perfetto/src/base/time.cc',
        'perfetto/src/base/uuid.cc',
        'perfetto/src/base/virtual_destructors.cc',
        'perfetto/src/base/waitable_event.cc',
        'perfetto/src/base/watchdog_posix.cc',
        'perfetto/src/base/unix_socket.cc',

        # base (posix)
        'perfetto/src/base/event_fd.cc',
        'perfetto/src/base/pipe.cc',
        'perfetto/src/base/temp_file.cc',
        'perfetto/src/base/thread_task_runner.cc',
        'perfetto/src/base/unix_task_runner.cc',

        'perfetto/src/ipc/protoc_plugin/ipc_plugin.cc',
      ]
    },
    {
      'target_name': 'protozero_plugin',
      'type': 'executable',
      'dependencies': [
        '../protobuf/protobuf.gyp:libprotoc',
      ],
      'defines': [
        'GOOGLE_PROTOBUF_NO_RTTI',
        'GOOGLE_PROTOBUF_NO_STATIC_INITIALIZER',
      ],
      'include_dirs': [
        'config',
        'perfetto/include',
        'perfetto/src',
        'perfetto',
      ],
      'sources': [
        'perfetto/src/base/file_utils.cc',
        'perfetto/src/base/logging.cc',
        'perfetto/src/base/metatrace.cc',
        'perfetto/src/base/paged_memory.cc',
        'perfetto/src/base/string_splitter.cc',
        'perfetto/src/base/string_utils.cc',
        'perfetto/src/base/string_view.cc',
        'perfetto/src/base/subprocess.cc',
        'perfetto/src/base/thread_checker.cc',
        'perfetto/src/base/time.cc',
        'perfetto/src/base/uuid.cc',
        'perfetto/src/base/virtual_destructors.cc',
        'perfetto/src/base/waitable_event.cc',
        'perfetto/src/base/watchdog_posix.cc',
        'perfetto/src/base/unix_socket.cc',

        # base (posix)
        'perfetto/src/base/event_fd.cc',
        'perfetto/src/base/pipe.cc',
        'perfetto/src/base/temp_file.cc',
        'perfetto/src/base/thread_task_runner.cc',
        'perfetto/src/base/unix_task_runner.cc',

        'perfetto/src/protozero/protoc_plugin/protozero_plugin.cc',
      ]
    },
    {
      'target_name': 'cppgen_plugin',
      'type': 'executable',
      'dependencies': [
        '../protobuf/protobuf.gyp:libprotoc',
      ],
      'defines': [
        'GOOGLE_PROTOBUF_NO_RTTI',
        'GOOGLE_PROTOBUF_NO_STATIC_INITIALIZER',
      ],
      'include_dirs': [
        'config',
        'perfetto/include',
        'perfetto/src',
        'perfetto',
      ],
      'sources': [
        'perfetto/src/base/file_utils.cc',
        'perfetto/src/base/logging.cc',
        'perfetto/src/base/metatrace.cc',
        'perfetto/src/base/paged_memory.cc',
        'perfetto/src/base/string_splitter.cc',
        'perfetto/src/base/string_utils.cc',
        'perfetto/src/base/string_view.cc',
        'perfetto/src/base/subprocess.cc',
        'perfetto/src/base/thread_checker.cc',
        'perfetto/src/base/time.cc',
        'perfetto/src/base/uuid.cc',
        'perfetto/src/base/virtual_destructors.cc',
        'perfetto/src/base/waitable_event.cc',
        'perfetto/src/base/watchdog_posix.cc',
        'perfetto/src/base/unix_socket.cc',

        # base (posix)
        'perfetto/src/base/event_fd.cc',
        'perfetto/src/base/pipe.cc',
        'perfetto/src/base/temp_file.cc',
        'perfetto/src/base/thread_task_runner.cc',
        'perfetto/src/base/unix_task_runner.cc',

        'perfetto/src/protozero/protoc_plugin/cppgen_plugin.cc',
      ]
    },
    {
      'target_name': 'protoc-gen',
      'type': 'none',
      'actions': [
        {
          'action_name': 'protozero-gen',
          'inputs': [
            'perfetto',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/perfetto/protozero',
          ],
          'action': [
            'tools/protoc.py',
            '-b<(PRODUCT_DIR)',
            '-Iperfetto',
            '--plugin=protozero',
            '-d<(SHARED_INTERMEDIATE_DIR)/perfetto/protozero'
          ],
        },
        {
          'action_name': 'ipc-gen',
          'inputs': [
            'perfetto',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/perfetto/ipc',
          ],
          'action': [
            'tools/protoc.py',
            '-b<(PRODUCT_DIR)',
            '-Iperfetto',
            '--plugin=ipc',
            '-d<(SHARED_INTERMEDIATE_DIR)/perfetto/ipc'
          ],
        },
        {
          'action_name': 'cpp-gen',
          'inputs': [
            'perfetto',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/perfetto/cppgen',
          ],
          'action': [
            'tools/protoc.py',
            '-b<(PRODUCT_DIR)',
            '-Iperfetto',
            '--plugin=cppgen',
            '-d<(SHARED_INTERMEDIATE_DIR)/perfetto/cppgen'
          ],
        },
        {
          'action_name': 'cpp',
          'inputs': [
            'perfetto',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/perfetto/cpp',
          ],
          'action': [
            'tools/protoc.py',
            '-b<(PRODUCT_DIR)',
            '-Iperfetto',
            '-d<(SHARED_INTERMEDIATE_DIR)/perfetto/cpp'
          ],
        }
      ],
    }
  ]
}
