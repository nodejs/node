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

#include "src/profiling/perf/event_reader.h"

#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#include "perfetto/ext/base/utils.h"
#include "src/profiling/perf/regs_parsing.h"

namespace perfetto {
namespace profiling {

namespace {

template <typename T>
const char* ReadValue(T* value_out, const char* ptr) {
  memcpy(value_out, reinterpret_cast<const void*>(ptr), sizeof(T));
  return ptr + sizeof(T);
}

bool IsPowerOfTwo(size_t v) {
  return (v != 0 && ((v & (v - 1)) == 0));
}

static int perf_event_open(perf_event_attr* attr,
                           pid_t pid,
                           int cpu,
                           int group_fd,
                           unsigned long flags) {
  return static_cast<int>(
      syscall(__NR_perf_event_open, attr, pid, cpu, group_fd, flags));
}

base::ScopedFile PerfEventOpen(uint32_t cpu, const EventConfig& event_cfg) {
  base::ScopedFile perf_fd{
      perf_event_open(event_cfg.perf_attr(), /*pid=*/-1, static_cast<int>(cpu),
                      /*group_fd=*/-1, PERF_FLAG_FD_CLOEXEC)};
  return perf_fd;
}

}  // namespace

PerfRingBuffer::PerfRingBuffer(PerfRingBuffer&& other) noexcept
    : metadata_page_(other.metadata_page_),
      mmap_sz_(other.mmap_sz_),
      data_buf_(other.data_buf_),
      data_buf_sz_(other.data_buf_sz_) {
  other.metadata_page_ = nullptr;
  other.mmap_sz_ = 0;
  other.data_buf_ = nullptr;
  other.data_buf_sz_ = 0;
}

PerfRingBuffer& PerfRingBuffer::operator=(PerfRingBuffer&& other) noexcept {
  if (this == &other)
    return *this;

  this->~PerfRingBuffer();
  new (this) PerfRingBuffer(std::move(other));
  return *this;
}

PerfRingBuffer::~PerfRingBuffer() {
  if (!valid())
    return;

  if (munmap(reinterpret_cast<void*>(metadata_page_), mmap_sz_) != 0)
    PERFETTO_PLOG("failed munmap");
}

base::Optional<PerfRingBuffer> PerfRingBuffer::Allocate(
    int perf_fd,
    size_t data_page_count) {
  // perf_event_open requires the ring buffer to be a power of two in size.
  PERFETTO_DCHECK(IsPowerOfTwo(data_page_count));

  PerfRingBuffer ret;

  // mmap request is one page larger than the buffer size (for the metadata).
  ret.data_buf_sz_ = data_page_count * base::kPageSize;
  ret.mmap_sz_ = ret.data_buf_sz_ + base::kPageSize;

  // If PROT_WRITE, kernel won't overwrite unread samples.
  void* mmap_addr = mmap(nullptr, ret.mmap_sz_, PROT_READ | PROT_WRITE,
                         MAP_SHARED, perf_fd, 0);
  if (mmap_addr == MAP_FAILED) {
    PERFETTO_PLOG("failed mmap");
    return base::nullopt;
  }

  // Expected layout is [ metadata page ] [ data pages ... ]
  ret.metadata_page_ = reinterpret_cast<perf_event_mmap_page*>(mmap_addr);
  ret.data_buf_ = reinterpret_cast<char*>(mmap_addr) + base::kPageSize;
  PERFETTO_CHECK(ret.metadata_page_->data_offset == base::kPageSize);
  PERFETTO_CHECK(ret.metadata_page_->data_size = ret.data_buf_sz_);

  return base::make_optional(std::move(ret));
}

// See |perf_output_put_handle| for the necessary synchronization between the
// kernel and this userspace thread (which are using the same shared memory, but
// might be on different cores).
// TODO(rsavitski): is there false sharing between |data_tail| and |data_head|?
// Is there an argument for maintaining our own copy of |data_tail| instead of
// reloading it?
char* PerfRingBuffer::ReadRecordNonconsuming() {
  static_assert(sizeof(std::atomic<uint64_t>) == sizeof(uint64_t), "");

  PERFETTO_CHECK(valid());

  // |data_tail| is written only by this userspace thread, so we can safely read
  // it without any synchronization.
  uint64_t read_offset = metadata_page_->data_tail;

  // |data_head| is written by the kernel, perform an acquiring load such that
  // the payload reads below are ordered after this load.
  uint64_t write_offset =
      reinterpret_cast<std::atomic<uint64_t>*>(&metadata_page_->data_head)
          ->load(std::memory_order_acquire);

  PERFETTO_DCHECK(read_offset <= write_offset);
  if (write_offset == read_offset)
    return nullptr;  // no new data

  size_t read_pos = static_cast<size_t>(read_offset & (data_buf_sz_ - 1));

  // event header (64 bits) guaranteed to be contiguous
  PERFETTO_DCHECK(read_pos <= data_buf_sz_ - sizeof(perf_event_header));
  PERFETTO_DCHECK(0 == reinterpret_cast<size_t>(data_buf_ + read_pos) %
                           alignof(perf_event_header));

  perf_event_header* evt_header =
      reinterpret_cast<perf_event_header*>(data_buf_ + read_pos);
  uint16_t evt_size = evt_header->size;

  // event wrapped - reconstruct it, and return a pointer to the buffer
  if (read_pos + evt_size > data_buf_sz_) {
    PERFETTO_DCHECK(read_pos + evt_size !=
                    ((read_pos + evt_size) & (data_buf_sz_ - 1)));
    PERFETTO_DLOG("PerfRingBuffer: returning reconstructed event");

    size_t prefix_sz = data_buf_sz_ - read_pos;
    memcpy(&reconstructed_record_[0], data_buf_ + read_pos, prefix_sz);
    memcpy(&reconstructed_record_[0] + prefix_sz, data_buf_,
           evt_size - prefix_sz);
    return &reconstructed_record_[0];
  } else {
    // usual case - contiguous sample
    PERFETTO_DCHECK(read_pos + evt_size ==
                    ((read_pos + evt_size) & (data_buf_sz_ - 1)));

    return data_buf_ + read_pos;
  }
}

void PerfRingBuffer::Consume(size_t bytes) {
  PERFETTO_CHECK(valid());

  // Advance |data_tail|, which is written only by this thread. The store of the
  // updated value needs to have release semantics such that the preceding
  // payload reads are ordered before it. The reader in this case is the kernel,
  // which reads |data_tail| to calculate the available ring buffer capacity
  // before trying to store a new record.
  uint64_t updated_tail = metadata_page_->data_tail + bytes;
  reinterpret_cast<std::atomic<uint64_t>*>(&metadata_page_->data_tail)
      ->store(updated_tail, std::memory_order_release);
}

EventReader::EventReader(uint32_t cpu,
                         perf_event_attr event_attr,
                         base::ScopedFile perf_fd,
                         PerfRingBuffer ring_buffer)
    : cpu_(cpu),
      event_attr_(event_attr),
      perf_fd_(std::move(perf_fd)),
      ring_buffer_(std::move(ring_buffer)) {}

EventReader::EventReader(EventReader&& other) noexcept
    : cpu_(other.cpu_),
      event_attr_(other.event_attr_),
      perf_fd_(std::move(other.perf_fd_)),
      ring_buffer_(std::move(other.ring_buffer_)) {}

EventReader& EventReader::operator=(EventReader&& other) noexcept {
  if (this == &other)
    return *this;

  this->~EventReader();
  new (this) EventReader(std::move(other));
  return *this;
}

base::Optional<EventReader> EventReader::ConfigureEvents(
    uint32_t cpu,
    const EventConfig& event_cfg) {
  auto perf_fd = PerfEventOpen(cpu, event_cfg);
  if (!perf_fd) {
    PERFETTO_PLOG("failed perf_event_open");
    return base::nullopt;
  }

  auto ring_buffer =
      PerfRingBuffer::Allocate(perf_fd.get(), event_cfg.ring_buffer_pages());
  if (!ring_buffer.has_value()) {
    return base::nullopt;
  }

  return base::make_optional<EventReader>(cpu, *event_cfg.perf_attr(),
                                          std::move(perf_fd),
                                          std::move(ring_buffer.value()));
}

base::Optional<ParsedSample> EventReader::ReadUntilSample(
    std::function<void(uint64_t)> records_lost_callback) {
  for (;;) {
    char* event = ring_buffer_.ReadRecordNonconsuming();
    if (!event)
      return base::nullopt;  // caught up with the writer

    auto* event_hdr = reinterpret_cast<const perf_event_header*>(event);

    if (event_hdr->type == PERF_RECORD_SAMPLE) {
      ParsedSample sample = ParseSampleRecord(cpu_, event);
      ring_buffer_.Consume(event_hdr->size);
      return base::make_optional(std::move(sample));
    }

    if (event_hdr->type == PERF_RECORD_LOST) {
      /*
       * struct {
       *   struct perf_event_header header;
       *   u64 id;
       *   u64 lost;
       *   struct sample_id sample_id;
       * };
       */
      uint64_t records_lost = *reinterpret_cast<const uint64_t*>(
          event + sizeof(perf_event_header) + sizeof(uint64_t));

      records_lost_callback(records_lost);
      ring_buffer_.Consume(event_hdr->size);
      continue;  // keep looking for a sample
    }

    // Kernel had to throttle irqs.
    if (event_hdr->type == PERF_RECORD_THROTTLE ||
        event_hdr->type == PERF_RECORD_UNTHROTTLE) {
      ring_buffer_.Consume(event_hdr->size);
      continue;  // keep looking for a sample
    }

    PERFETTO_DFATAL_OR_ELOG("Unsupported event type [%zu]",
                            static_cast<size_t>(event_hdr->type));
    ring_buffer_.Consume(event_hdr->size);
  }
}

// Generally, samples can belong to any cpu (which can be recorded with
// PERF_SAMPLE_CPU). However, this producer uses only cpu-scoped events,
// therefore it is already known.
ParsedSample EventReader::ParseSampleRecord(uint32_t cpu,
                                            const char* record_start) {
  if (event_attr_.sample_type &
      (~uint64_t(PERF_SAMPLE_TID | PERF_SAMPLE_TIME | PERF_SAMPLE_STACK_USER |
                 PERF_SAMPLE_REGS_USER))) {
    PERFETTO_FATAL("Unsupported sampling option");
  }

  auto* event_hdr = reinterpret_cast<const perf_event_header*>(record_start);
  size_t sample_size = event_hdr->size;

  ParsedSample sample = {};
  sample.cpu = cpu;
  sample.cpu_mode = event_hdr->misc & PERF_RECORD_MISC_CPUMODE_MASK;

  // Parse the payload, which consists of concatenated data for each
  // |attr.sample_type| flag.
  const char* parse_pos = record_start + sizeof(perf_event_header);

  if (event_attr_.sample_type & PERF_SAMPLE_TID) {
    uint32_t pid = 0;
    uint32_t tid = 0;
    parse_pos = ReadValue(&pid, parse_pos);
    parse_pos = ReadValue(&tid, parse_pos);
    sample.pid = static_cast<pid_t>(pid);
    sample.tid = static_cast<pid_t>(tid);
  }

  if (event_attr_.sample_type & PERF_SAMPLE_TIME) {
    parse_pos = ReadValue(&sample.timestamp, parse_pos);
  }

  if (event_attr_.sample_type & PERF_SAMPLE_REGS_USER) {
    // Can be empty, e.g. if we sampled a kernel thread.
    sample.regs = ReadPerfUserRegsData(&parse_pos);
  }

  if (event_attr_.sample_type & PERF_SAMPLE_STACK_USER) {
    uint64_t max_stack_size;  // the requested size
    parse_pos = ReadValue(&max_stack_size, parse_pos);

    const char* stack_start = parse_pos;
    parse_pos += max_stack_size;  // skip to dyn_size

    // Payload written conditionally, e.g. kernel threads don't have a
    // user stack.
    if (max_stack_size > 0) {
      uint64_t filled_stack_size;
      parse_pos = ReadValue(&filled_stack_size, parse_pos);
      PERFETTO_DLOG("sampled stack size: %" PRIu64 " / %" PRIu64 "",
                    filled_stack_size, max_stack_size);

      // copy stack bytes into a vector
      size_t payload_sz = static_cast<size_t>(filled_stack_size);
      sample.stack.resize(payload_sz);
      memcpy(sample.stack.data(), stack_start, payload_sz);
    }
  }

  PERFETTO_CHECK(parse_pos == record_start + sample_size);
  return sample;
}

void EventReader::PauseEvents() {
  int ret = ioctl(perf_fd_.get(), PERF_EVENT_IOC_DISABLE);
  PERFETTO_CHECK(ret == 0);
}

}  // namespace profiling
}  // namespace perfetto
