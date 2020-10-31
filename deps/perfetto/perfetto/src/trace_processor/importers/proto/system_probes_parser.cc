/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "src/trace_processor/importers/proto/system_probes_parser.h"

#include <set>

#include "perfetto/base/logging.h"
#include "perfetto/ext/traced/sys_stats_counters.h"
#include "perfetto/protozero/proto_decoder.h"
#include "src/trace_processor/importers/common/event_tracker.h"
#include "src/trace_processor/importers/common/process_tracker.h"
#include "src/trace_processor/importers/proto/metadata_tracker.h"
#include "src/trace_processor/importers/syscalls/syscall_tracker.h"
#include "src/trace_processor/storage/metadata.h"
#include "src/trace_processor/types/trace_processor_context.h"

#include "protos/perfetto/trace/ps/process_stats.pbzero.h"
#include "protos/perfetto/trace/ps/process_tree.pbzero.h"
#include "protos/perfetto/trace/sys_stats/sys_stats.pbzero.h"
#include "protos/perfetto/trace/system_info.pbzero.h"
#include "protos/perfetto/trace/system_info/cpu_info.pbzero.h"

namespace perfetto {
namespace trace_processor {

namespace {
// kthreadd is the parent process for all kernel threads and always has
// pid == 2 on Linux and Android.
const uint32_t kKthreaddPid = 2;
const char kKthreaddName[] = "kthreadd";
}  // namespace

SystemProbesParser::SystemProbesParser(TraceProcessorContext* context)
    : context_(context),
      utid_name_id_(context->storage->InternString("utid")),
      num_forks_name_id_(context->storage->InternString("num_forks")),
      num_irq_total_name_id_(context->storage->InternString("num_irq_total")),
      num_softirq_total_name_id_(
          context->storage->InternString("num_softirq_total")),
      num_irq_name_id_(context->storage->InternString("num_irq")),
      num_softirq_name_id_(context->storage->InternString("num_softirq")),
      cpu_times_user_ns_id_(
          context->storage->InternString("cpu.times.user_ns")),
      cpu_times_user_nice_ns_id_(
          context->storage->InternString("cpu.times.user_nice_ns")),
      cpu_times_system_mode_ns_id_(
          context->storage->InternString("cpu.times.system_mode_ns")),
      cpu_times_idle_ns_id_(
          context->storage->InternString("cpu.times.idle_ns")),
      cpu_times_io_wait_ns_id_(
          context->storage->InternString("cpu.times.io_wait_ns")),
      cpu_times_irq_ns_id_(context->storage->InternString("cpu.times.irq_ns")),
      cpu_times_softirq_ns_id_(
          context->storage->InternString("cpu.times.softirq_ns")),
      oom_score_adj_id_(context->storage->InternString("oom_score_adj")) {
  for (const auto& name : BuildMeminfoCounterNames()) {
    meminfo_strs_id_.emplace_back(context->storage->InternString(name));
  }
  for (const auto& name : BuildVmstatCounterNames()) {
    vmstat_strs_id_.emplace_back(context->storage->InternString(name));
  }

  using ProcessStats = protos::pbzero::ProcessStats;
  proc_stats_process_names_[ProcessStats::Process::kVmSizeKbFieldNumber] =
      context->storage->InternString("mem.virt");
  proc_stats_process_names_[ProcessStats::Process::kVmRssKbFieldNumber] =
      context->storage->InternString("mem.rss");
  proc_stats_process_names_[ProcessStats::Process::kRssAnonKbFieldNumber] =
      context->storage->InternString("mem.rss.anon");
  proc_stats_process_names_[ProcessStats::Process::kRssFileKbFieldNumber] =
      context->storage->InternString("mem.rss.file");
  proc_stats_process_names_[ProcessStats::Process::kRssShmemKbFieldNumber] =
      context->storage->InternString("mem.rss.shmem");
  proc_stats_process_names_[ProcessStats::Process::kVmSwapKbFieldNumber] =
      context->storage->InternString("mem.swap");
  proc_stats_process_names_[ProcessStats::Process::kVmLockedKbFieldNumber] =
      context->storage->InternString("mem.locked");
  proc_stats_process_names_[ProcessStats::Process::kVmHwmKbFieldNumber] =
      context->storage->InternString("mem.rss.watermark");
  proc_stats_process_names_[ProcessStats::Process::kOomScoreAdjFieldNumber] =
      oom_score_adj_id_;
}

void SystemProbesParser::ParseSysStats(int64_t ts, ConstBytes blob) {
  protos::pbzero::SysStats::Decoder sys_stats(blob.data, blob.size);

  for (auto it = sys_stats.meminfo(); it; ++it) {
    protos::pbzero::SysStats::MeminfoValue::Decoder mi(*it);
    auto key = static_cast<size_t>(mi.key());
    if (PERFETTO_UNLIKELY(key >= meminfo_strs_id_.size())) {
      PERFETTO_ELOG("MemInfo key %zu is not recognized.", key);
      context_->storage->IncrementStats(stats::meminfo_unknown_keys);
      continue;
    }
    // /proc/meminfo counters are in kB, convert to bytes
    TrackId track = context_->track_tracker->InternGlobalCounterTrack(
        meminfo_strs_id_[key]);
    context_->event_tracker->PushCounter(ts, mi.value() * 1024L, track);
  }

  for (auto it = sys_stats.vmstat(); it; ++it) {
    protos::pbzero::SysStats::VmstatValue::Decoder vm(*it);
    auto key = static_cast<size_t>(vm.key());
    if (PERFETTO_UNLIKELY(key >= vmstat_strs_id_.size())) {
      PERFETTO_ELOG("VmStat key %zu is not recognized.", key);
      context_->storage->IncrementStats(stats::vmstat_unknown_keys);
      continue;
    }
    TrackId track =
        context_->track_tracker->InternGlobalCounterTrack(vmstat_strs_id_[key]);
    context_->event_tracker->PushCounter(ts, vm.value(), track);
  }

  for (auto it = sys_stats.cpu_stat(); it; ++it) {
    protos::pbzero::SysStats::CpuTimes::Decoder ct(*it);
    if (PERFETTO_UNLIKELY(!ct.has_cpu_id())) {
      PERFETTO_ELOG("CPU field not found in CpuTimes");
      context_->storage->IncrementStats(stats::invalid_cpu_times);
      continue;
    }

    TrackId track = context_->track_tracker->InternCpuCounterTrack(
        cpu_times_user_ns_id_, ct.cpu_id());
    context_->event_tracker->PushCounter(ts, ct.user_ns(), track);

    track = context_->track_tracker->InternCpuCounterTrack(
        cpu_times_user_nice_ns_id_, ct.cpu_id());
    context_->event_tracker->PushCounter(ts, ct.user_ice_ns(), track);

    track = context_->track_tracker->InternCpuCounterTrack(
        cpu_times_system_mode_ns_id_, ct.cpu_id());
    context_->event_tracker->PushCounter(ts, ct.system_mode_ns(), track);

    track = context_->track_tracker->InternCpuCounterTrack(
        cpu_times_idle_ns_id_, ct.cpu_id());
    context_->event_tracker->PushCounter(ts, ct.idle_ns(), track);

    track = context_->track_tracker->InternCpuCounterTrack(
        cpu_times_io_wait_ns_id_, ct.cpu_id());
    context_->event_tracker->PushCounter(ts, ct.io_wait_ns(), track);

    track = context_->track_tracker->InternCpuCounterTrack(cpu_times_irq_ns_id_,
                                                           ct.cpu_id());
    context_->event_tracker->PushCounter(ts, ct.irq_ns(), track);

    track = context_->track_tracker->InternCpuCounterTrack(
        cpu_times_softirq_ns_id_, ct.cpu_id());
    context_->event_tracker->PushCounter(ts, ct.softirq_ns(), track);
  }

  for (auto it = sys_stats.num_irq(); it; ++it) {
    protos::pbzero::SysStats::InterruptCount::Decoder ic(*it);

    TrackId track = context_->track_tracker->InternIrqCounterTrack(
        num_irq_name_id_, ic.irq());
    context_->event_tracker->PushCounter(ts, ic.count(), track);
  }

  for (auto it = sys_stats.num_softirq(); it; ++it) {
    protos::pbzero::SysStats::InterruptCount::Decoder ic(*it);

    TrackId track = context_->track_tracker->InternSoftirqCounterTrack(
        num_softirq_name_id_, ic.irq());
    context_->event_tracker->PushCounter(ts, ic.count(), track);
  }

  if (sys_stats.has_num_forks()) {
    TrackId track =
        context_->track_tracker->InternGlobalCounterTrack(num_forks_name_id_);
    context_->event_tracker->PushCounter(ts, sys_stats.num_forks(), track);
  }

  if (sys_stats.has_num_irq_total()) {
    TrackId track = context_->track_tracker->InternGlobalCounterTrack(
        num_irq_total_name_id_);
    context_->event_tracker->PushCounter(ts, sys_stats.num_irq_total(), track);
  }

  if (sys_stats.has_num_softirq_total()) {
    TrackId track = context_->track_tracker->InternGlobalCounterTrack(
        num_softirq_total_name_id_);
    context_->event_tracker->PushCounter(ts, sys_stats.num_softirq_total(),
                                         track);
  }
}

void SystemProbesParser::ParseProcessTree(ConstBytes blob) {
  protos::pbzero::ProcessTree::Decoder ps(blob.data, blob.size);

  for (auto it = ps.processes(); it; ++it) {
    protos::pbzero::ProcessTree::Process::Decoder proc(*it);
    if (!proc.has_cmdline())
      continue;
    auto pid = static_cast<uint32_t>(proc.pid());
    auto ppid = static_cast<uint32_t>(proc.ppid());

    // If the parent pid is kthreadd's pid, even though this pid is of a
    // "process", we want to treat it as being a child thread of kthreadd.
    if (ppid == kKthreaddPid) {
      context_->process_tracker->SetProcessMetadata(kKthreaddPid, base::nullopt,
                                                    kKthreaddName);
      context_->process_tracker->UpdateThread(pid, kKthreaddPid);
    } else {
      auto args = proc.cmdline();
      base::StringView argv0 = args ? *args : base::StringView();
      UniquePid upid =
          context_->process_tracker->SetProcessMetadata(pid, ppid, argv0);
      if (proc.has_uid()) {
        context_->process_tracker->SetProcessUid(
            upid, static_cast<uint32_t>(proc.uid()));
      }
    }
  }

  for (auto it = ps.threads(); it; ++it) {
    protos::pbzero::ProcessTree::Thread::Decoder thd(*it);
    auto tid = static_cast<uint32_t>(thd.tid());
    auto tgid = static_cast<uint32_t>(thd.tgid());
    context_->process_tracker->UpdateThread(tid, tgid);

    if (thd.has_name()) {
      StringId thread_name_id = context_->storage->InternString(thd.name());
      context_->process_tracker->UpdateThreadName(tid, thread_name_id);
    }
  }
}

void SystemProbesParser::ParseProcessStats(int64_t ts, ConstBytes blob) {
  protos::pbzero::ProcessStats::Decoder stats(blob.data, blob.size);
  const auto kOomScoreAdjFieldNumber =
      protos::pbzero::ProcessStats::Process::kOomScoreAdjFieldNumber;
  for (auto it = stats.processes(); it; ++it) {
    // Maps a process counter field it to its value.
    // E.g., 4 := 1024 -> "mem.rss.anon" := 1024.
    std::array<int64_t, kProcStatsProcessSize> counter_values{};
    std::array<bool, kProcStatsProcessSize> has_counter{};

    protozero::ProtoDecoder proc(*it);
    uint32_t pid = 0;
    for (auto fld = proc.ReadField(); fld.valid(); fld = proc.ReadField()) {
      if (fld.id() == protos::pbzero::ProcessStats::Process::kPidFieldNumber) {
        pid = fld.as_uint32();
        continue;
      }
      if (fld.id() ==
          protos::pbzero::ProcessStats::Process::kThreadsFieldNumber) {
        if (PERFETTO_UNLIKELY(ms_per_tick_ == 0 ||
                              thread_time_in_state_cpu_str_ids_.empty())) {
          context_->storage->IncrementStats(
              stats::thread_time_in_state_out_of_order);
          continue;
        }
        ParseThreadStats(ts, pid, fld.as_bytes());
        continue;
      }
      bool is_counter_field = fld.id() < proc_stats_process_names_.size() &&
                              !proc_stats_process_names_[fld.id()].is_null();
      if (is_counter_field) {
        // Memory counters are in KB, keep values in bytes in the trace
        // processor.
        counter_values[fld.id()] = fld.id() == kOomScoreAdjFieldNumber
                                       ? fld.as_int64()
                                       : fld.as_int64() * 1024;
        has_counter[fld.id()] = true;
      } else {
        context_->storage->IncrementStats(stats::proc_stat_unknown_counters);
      }
    }

    // Skip field_id 0 (invalid) and 1 (pid).
    for (size_t field_id = 2; field_id < counter_values.size(); field_id++) {
      if (!has_counter[field_id])
        continue;

      // Lookup the interned string id from the field name using the
      // pre-cached |proc_stats_process_names_| map.
      StringId name = proc_stats_process_names_[field_id];
      int64_t value = counter_values[field_id];
      UniquePid upid = context_->process_tracker->GetOrCreateProcess(pid);
      TrackId track =
          context_->track_tracker->InternProcessCounterTrack(name, upid);
      context_->event_tracker->PushCounter(ts, value, track);
    }
  }
}

void SystemProbesParser::ParseThreadStats(int64_t ts,
                                          uint32_t pid,
                                          ConstBytes blob) {
  protos::pbzero::ProcessStats::Thread::Decoder stats(blob.data, blob.size);
  UniqueTid utid = context_->process_tracker->UpdateThread(
      static_cast<uint32_t>(stats.tid()), pid);
  TrackId track_id = context_->track_tracker->InternThreadTrack(utid);

  std::set<StringId> cpu_str_ids;
  for (auto index_it = stats.cpu_freq_indices(); index_it; index_it++) {
    auto freq_index = *index_it;
    if (PERFETTO_UNLIKELY(freq_index == 0 ||
                          freq_index >=
                              thread_time_in_state_cpu_str_ids_.size())) {
      context_->storage->IncrementStats(
          stats::thread_time_in_state_unknown_cpu_freq);
      continue;
    }
    cpu_str_ids.insert(thread_time_in_state_cpu_str_ids_[freq_index]);
  }

  for (StringId cpu_str_id : cpu_str_ids) {
    context_->slice_tracker->Scoped(
        ts, track_id, kNullStringId, cpu_str_id,
        /* duration */ 0,
        [&stats, &cpu_str_id, this](ArgsTracker::BoundInserter* args_table) {
          auto index_it = stats.cpu_freq_indices();
          auto tick_it = stats.cpu_freq_ticks();
          for (; index_it && tick_it; index_it++, tick_it++) {
            auto freq_index = *index_it;
            if (PERFETTO_UNLIKELY(
                    freq_index == 0 ||
                    freq_index >= thread_time_in_state_cpu_str_ids_.size())) {
              continue;
            }
            if (thread_time_in_state_cpu_str_ids_[freq_index] != cpu_str_id)
              continue;
            args_table->AddArg(
                thread_time_in_state_cpu_freq_ids_[*index_it],
                Variadic::UnsignedInteger(*tick_it * ms_per_tick_));
          }
        });
  }
}

void SystemProbesParser::ParseSystemInfo(ConstBytes blob) {
  protos::pbzero::SystemInfo::Decoder packet(blob.data, blob.size);
  if (packet.has_utsname()) {
    ConstBytes utsname_blob = packet.utsname();
    protos::pbzero::Utsname::Decoder utsname(utsname_blob.data,
                                             utsname_blob.size);
    base::StringView machine = utsname.machine();
    SyscallTracker* syscall_tracker = SyscallTracker::GetOrCreate(context_);
    if (machine == "aarch64" || machine == "armv8l") {
      syscall_tracker->SetArchitecture(kAarch64);
    } else if (machine == "x86_64") {
      syscall_tracker->SetArchitecture(kX86_64);
    } else if (machine == "i686") {
      syscall_tracker->SetArchitecture(kX86);
    } else {
      PERFETTO_ELOG("Unknown architecture %s", machine.ToStdString().c_str());
    }

    StringPool::Id sysname_id =
        context_->storage->InternString(utsname.sysname());
    StringPool::Id version_id =
        context_->storage->InternString(utsname.version());
    StringPool::Id release_id =
        context_->storage->InternString(utsname.release());
    StringPool::Id machine_id =
        context_->storage->InternString(utsname.machine());

    MetadataTracker* metadata = context_->metadata_tracker.get();
    metadata->SetMetadata(metadata::system_name, Variadic::String(sysname_id));
    metadata->SetMetadata(metadata::system_version,
                          Variadic::String(version_id));
    metadata->SetMetadata(metadata::system_release,
                          Variadic::String(release_id));
    metadata->SetMetadata(metadata::system_machine,
                          Variadic::String(machine_id));
  }

  if (packet.has_android_build_fingerprint()) {
    context_->metadata_tracker->SetMetadata(
        metadata::android_build_fingerprint,
        Variadic::String(context_->storage->InternString(
            packet.android_build_fingerprint())));
  }

  int64_t hz = packet.hz();
  if (hz > 0)
    ms_per_tick_ = 1000u / static_cast<uint64_t>(hz);
}

void SystemProbesParser::ParseCpuInfo(ConstBytes blob) {
  // Frequency index 0 is invalid.
  StringId invalid_str_id = context_->storage->InternString("invalid");
  thread_time_in_state_cpu_str_ids_.push_back(invalid_str_id);
  thread_time_in_state_cpu_freq_ids_.push_back(invalid_str_id);

  protos::pbzero::CpuInfo::Decoder packet(blob.data, blob.size);
  uint32_t cpu_index = 0;
  for (auto it = packet.cpus(); it; it++) {
    protos::pbzero::CpuInfo::Cpu::Decoder cpu(*it);
    std::string cpu_string = "time_in_state.cpu" + std::to_string(cpu_index);
    base::StringView cpu_string_view(cpu_string);
    for (auto freq_it = cpu.frequencies(); freq_it; freq_it++) {
      thread_time_in_state_cpu_str_ids_.push_back(
          context_->storage->InternString(cpu_string_view));
      std::string freq_string = std::to_string(*freq_it);
      base::StringView freq_string_view(freq_string);
      thread_time_in_state_cpu_freq_ids_.push_back(
          context_->storage->InternString(freq_string_view));
    }
    cpu_index++;
  }
}

}  // namespace trace_processor
}  // namespace perfetto
