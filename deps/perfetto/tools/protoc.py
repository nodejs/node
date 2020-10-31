#!/usr/bin/env python

import argparse
import fcntl
import os
import subprocess
import sys

sets = {
  'common': [
    '/protos/perfetto/common/android_log_constants.proto',
    '/protos/perfetto/common/commit_data_request.proto',
    '/protos/perfetto/common/data_source_descriptor.proto',
    '/protos/perfetto/common/descriptor.proto',
    '/protos/perfetto/common/gpu_counter_descriptor.proto',
    '/protos/perfetto/common/observable_events.proto',
    '/protos/perfetto/common/sys_stats_counters.proto',
    '/protos/perfetto/common/trace_stats.proto',
    '/protos/perfetto/common/tracing_service_capabilities.proto',
    '/protos/perfetto/common/tracing_service_state.proto',
    '/protos/perfetto/common/track_event_descriptor.proto',
  ],
  'config': [
    '/protos/perfetto/config/profiling/heapprofd_config.proto',
    '/protos/perfetto/config/profiling/perf_event_config.proto',
    '/protos/perfetto/config/profiling/java_hprof_config.proto',
    '/protos/perfetto/config/sys_stats/sys_stats_config.proto',
    '/protos/perfetto/config/trace_config.proto',
    '/protos/perfetto/config/inode_file/inode_file_config.proto',
    '/protos/perfetto/config/process_stats/process_stats_config.proto',
    '/protos/perfetto/config/ftrace/ftrace_config.proto',
    '/protos/perfetto/config/chrome/chrome_config.proto',
    '/protos/perfetto/config/test_config.proto',
    '/protos/perfetto/config/power/android_power_config.proto',
    '/protos/perfetto/config/android/packages_list_config.proto',
    '/protos/perfetto/config/android/android_log_config.proto',
    '/protos/perfetto/config/android/android_polled_state_config.proto',
    '/protos/perfetto/config/data_source_config.proto',
    '/protos/perfetto/config/track_event/track_event_config.proto',
    '/protos/perfetto/config/gpu/vulkan_memory_config.proto',
    '/protos/perfetto/config/gpu/gpu_counter_config.proto',
  ],
  'perfetto-config': [
    '/protos/perfetto/config/perfetto_config.proto',
  ],
  'ipc-wire-protocol': [
    '/protos/perfetto/ipc/wire_protocol.proto',
  ],
  'ipc-ports': [
    '/protos/perfetto/ipc/producer_port.proto',
    '/protos/perfetto/ipc/consumer_port.proto',
  ],
  'trace-profiling': [
    '/protos/perfetto/trace/profiling/smaps.proto',
    '/protos/perfetto/trace/profiling/profile_common.proto',
    '/protos/perfetto/trace/profiling/profile_packet.proto',
    '/protos/perfetto/trace/profiling/heap_graph.proto',
  ],
  'trace-sys-stats': [
    '/protos/perfetto/trace/sys_stats/sys_stats.proto',
  ],
  'trace-ps': [
    '/protos/perfetto/trace/ps/process_stats.proto',
    '/protos/perfetto/trace/ps/process_tree.proto',
  ],
  'trace-system-info': [
    '/protos/perfetto/trace/system_info/cpu_info.proto',
  ],
  'trace-ftrace': [
    '/protos/perfetto/trace/ftrace/sched.proto',
    '/protos/perfetto/trace/ftrace/mm_event.proto',
    '/protos/perfetto/trace/ftrace/oom.proto',
    '/protos/perfetto/trace/ftrace/sync.proto',
    '/protos/perfetto/trace/ftrace/kmem.proto',
    '/protos/perfetto/trace/ftrace/task.proto',
    '/protos/perfetto/trace/ftrace/power.proto',
    '/protos/perfetto/trace/ftrace/compaction.proto',
    '/protos/perfetto/trace/ftrace/systrace.proto',
    '/protos/perfetto/trace/ftrace/i2c.proto',
    '/protos/perfetto/trace/ftrace/ftrace_event.proto',
    '/protos/perfetto/trace/ftrace/ext4.proto',
    '/protos/perfetto/trace/ftrace/filemap.proto',
    '/protos/perfetto/trace/ftrace/sde.proto',
    '/protos/perfetto/trace/ftrace/irq.proto',
    '/protos/perfetto/trace/ftrace/clk.proto',
    '/protos/perfetto/trace/ftrace/fence.proto',
    '/protos/perfetto/trace/ftrace/mdss.proto',
    '/protos/perfetto/trace/ftrace/generic.proto',
    '/protos/perfetto/trace/ftrace/test_bundle_wrapper.proto',
    '/protos/perfetto/trace/ftrace/binder.proto',
    '/protos/perfetto/trace/ftrace/f2fs.proto',
    '/protos/perfetto/trace/ftrace/lowmemorykiller.proto',
    '/protos/perfetto/trace/ftrace/regulator.proto',
    '/protos/perfetto/trace/ftrace/workqueue.proto',
    '/protos/perfetto/trace/ftrace/ion.proto',
    '/protos/perfetto/trace/ftrace/raw_syscalls.proto',
    '/protos/perfetto/trace/ftrace/ipi.proto',
    '/protos/perfetto/trace/ftrace/ftrace.proto',
    '/protos/perfetto/trace/ftrace/block.proto',
    '/protos/perfetto/trace/ftrace/vmscan.proto',
    '/protos/perfetto/trace/ftrace/ftrace_stats.proto',
    '/protos/perfetto/trace/ftrace/cgroup.proto',
    '/protos/perfetto/trace/ftrace/ftrace_event_bundle.proto',
    '/protos/perfetto/trace/ftrace/signal.proto',
  ],
  'trace-chrome': [
    '/protos/perfetto/trace/chrome/chrome_trace_event.proto',
    '/protos/perfetto/trace/chrome/chrome_metadata.proto',
    '/protos/perfetto/trace/chrome/chrome_trace_packet.proto',
    '/protos/perfetto/trace/chrome/chrome_benchmark_metadata.proto',
  ],
  'trace-perfetto': [
    '/protos/perfetto/trace/perfetto/tracing_service_event.proto',
    '/protos/perfetto/trace/perfetto/perfetto_metatrace.proto',
  ],
  'trace-power': [
    '/protos/perfetto/trace/power/power_rails.proto',
    '/protos/perfetto/trace/power/battery_counters.proto',
  ],
  'trace-fs': [
    '/protos/perfetto/trace/filesystem/inode_file_map.proto',
  ],
  'trace-android': [
    '/protos/perfetto/trace/android/initial_display_state.proto',
    '/protos/perfetto/trace/android/packages_list.proto',
    '/protos/perfetto/trace/android/android_log.proto',
    '/protos/perfetto/trace/android/graphics_frame_event.proto',
  ],
  'trace-track-event': [
    '/protos/perfetto/trace/track_event/nodejs.proto',
    '/protos/perfetto/trace/track_event/thread_descriptor.proto',
    '/protos/perfetto/trace/track_event/chrome_thread_descriptor.proto',
    '/protos/perfetto/trace/track_event/track_descriptor.proto',
    '/protos/perfetto/trace/track_event/process_descriptor.proto',
    '/protos/perfetto/trace/track_event/chrome_legacy_ipc.proto',
    '/protos/perfetto/trace/track_event/chrome_histogram_sample.proto',
    '/protos/perfetto/trace/track_event/track_event.proto',
    '/protos/perfetto/trace/track_event/counter_descriptor.proto',
    '/protos/perfetto/trace/track_event/log_message.proto',
    '/protos/perfetto/trace/track_event/source_location.proto',
    '/protos/perfetto/trace/track_event/chrome_process_descriptor.proto',
    '/protos/perfetto/trace/track_event/chrome_user_event.proto',
    '/protos/perfetto/trace/track_event/chrome_compositor_scheduler_state.proto',
    '/protos/perfetto/trace/track_event/task_execution.proto',
    '/protos/perfetto/trace/track_event/debug_annotation.proto',
    '/protos/perfetto/trace/track_event/chrome_latency_info.proto',
    '/protos/perfetto/trace/track_event/chrome_keyed_service.proto',
  ],
  'trace-gpu': [
    '/protos/perfetto/trace/gpu/gpu_counter_event.proto',
    '/protos/perfetto/trace/gpu/gpu_render_stage_event.proto',
    '/protos/perfetto/trace/gpu/vulkan_memory_event.proto',
    '/protos/perfetto/trace/gpu/gpu_log.proto',
    '/protos/perfetto/trace/gpu/vulkan_api_event.proto',
  ],
  'trace': [
    '/protos/perfetto/trace/test_event.proto',
    '/protos/perfetto/trace/trace_packet_defaults.proto',
    '/protos/perfetto/trace/interned_data/interned_data.proto',
    '/protos/perfetto/trace/clock_snapshot.proto',
    '/protos/perfetto/trace/trace.proto',
    '/protos/perfetto/trace/trigger.proto',
    '/protos/perfetto/trace/trace_packet.proto',
    '/protos/perfetto/trace/system_info.proto',
  ],
  'trace-p': [
    '/protos/perfetto/trace/perfetto_trace.proto',
  ],
  'trace-processor': [
    '/protos/perfetto/trace_processor/trace_processor.proto',
    '/protos/perfetto/trace_processor/metrics_impl.proto',
  ],
  'metrics': [
    '/protos/perfetto/metrics/metrics.proto',
    '/protos/perfetto/metrics/custom_options.proto',
    #'/protos/perfetto/metrics/perfetto_merged_metrics.proto',
    '/protos/perfetto/metrics/android/unmapped_java_symbols.proto',
    '/protos/perfetto/metrics/android/mem_unagg_metric.proto',
    '/protos/perfetto/metrics/android/unsymbolized_frames.proto',
    '/protos/perfetto/metrics/android/ion_metric.proto',
    '/protos/perfetto/metrics/android/heap_profile_callsites.proto',
    '/protos/perfetto/metrics/android/mem_metric.proto',
    '/protos/perfetto/metrics/android/java_heap_stats.proto',
    '/protos/perfetto/metrics/android/java_heap_histogram.proto',
    '/protos/perfetto/metrics/android/batt_metric.proto',
    '/protos/perfetto/metrics/android/display_metrics.proto',
    '/protos/perfetto/metrics/android/package_list.proto',
    '/protos/perfetto/metrics/android/powrails_metric.proto',
    '/protos/perfetto/metrics/android/startup_metric.proto',
    '/protos/perfetto/metrics/android/task_names.proto',
    '/protos/perfetto/metrics/android/lmk_reason_metric.proto',
    '/protos/perfetto/metrics/android/hwui_metric.proto',
    '/protos/perfetto/metrics/android/cpu_metric.proto',
    '/protos/perfetto/metrics/android/lmk_metric.proto',
    '/protos/perfetto/metrics/android/thread_time_in_state_metric.proto',
    '/protos/perfetto/metrics/android/process_metadata.proto',
  ],
}

plugins = {
  'protozero': {
    'sets': [
      'common',
      'config',
      'perfetto-config',
      'ipc-ports',
      'trace-profiling',
      'trace-sys-stats',
      'trace-ps',
      'trace-system-info',
      'trace-ftrace',
      'trace-chrome',
      'trace-perfetto',
      'trace-power',
      'trace-fs',
      'trace-android',
      'trace-track-event',
      'trace-gpu',
      'trace',
      'trace-p'],
    'namespace': 'pbzero'
  },
  'ipc': {
    'sets': [ 'ipc-ports' ],
    'namespace': 'gen'
  },
  'cppgen': {
    'sets': [
      'common',
      'config',
      'perfetto-config',
      'ipc-wire-protocol',
      'ipc-ports',
      'trace-profiling',
      'trace-sys-stats',
      'trace-ps',
      'trace-system-info',
      'trace-ftrace',
      'trace-chrome',
      'trace-perfetto',
      'trace-power',
      'trace-fs',
      'trace-android',
      'trace-track-event',
      'trace-gpu',
      'trace',
      'trace-p'],
    'namespace': 'gen'
  }
}

default_sets = [
  'common',
  'config',
  'perfetto-config',
  'ipc-ports',
  'ipc-wire-protocol',
  'trace-profiling',
  'trace-sys-stats',
  'trace-ps',
  'trace-system-info',
  'trace-ftrace',
  'trace-chrome',
  'trace-perfetto',
  'trace-power',
  'trace-fs',
  'trace-android',
  'trace-track-event',
  'trace-gpu',
  'trace',
  'trace-p',
  'trace-processor',
  'metrics',
]

def parse_args():
  parser = argparse.ArgumentParser(description='protoc generation')
  parser.add_argument('--bin', '-b', metavar='path to the protoc binary')
  parser.add_argument('--protoc-path', '-I', metavar='protoc include path')
  parser.add_argument('--plugin', metavar='plugin to use')
  parser.add_argument('--dest', '-d', metavar='destination path')
  parser.add_argument('--set', '-s', metavar='the proto set to build')
  return parser.parse_args()

def protoc(args):
  if args.dest and not os.path.isdir(args.dest):
    os.makedirs(args.dest)

  res = 0
  if args.plugin is not None:
    print('Running protoc for plugin: {}'.format(args.plugin))
    for set in plugins[args.plugin]['sets']:
      cmd = [
        args.bin + '/protoc',
        '-I' + args.protoc_path,
        '--plugin=protoc-gen-plugin={}/{}_plugin'.format(args.bin, args.plugin),
        '--plugin_out=wrapper_namespace=' + plugins[args.plugin]['namespace'] + ':' + args.dest
      ] + [args.protoc_path + s for s in sets[set]]

      proc = subprocess.Popen(cmd)
      while True:
        if proc.poll() is not None:
          res = proc.returncode
          break
      if res is not 0:
        return res
  else:
    print('Running protoc for cpp_out')
    for set in default_sets:
      cmd = [
        args.bin + '/protoc',
        '-I' + args.protoc_path,
        '-I../protobuf/protobuf/src',
        '--cpp_out=' + args.dest,
      ] + [args.protoc_path + s for s in sets[set]]

      proc = subprocess.Popen(cmd)
      while True:
        if proc.poll() is not None:
          res = proc.returncode
          break
      if res is not 0:
        return res


  return 0

def main():
  return protoc(parse_args())

if __name__ == '__main__':
  sys.exit(main())
