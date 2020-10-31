/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include "src/perfetto_cmd/perfetto_cmd.h"

#include <inttypes.h>
#include <sys/sendfile.h>

#include "perfetto/base/build_config.h"
#include "perfetto/base/logging.h"
#include "perfetto/ext/base/uuid.h"
#include "perfetto/tracing/core/trace_config.h"
#include "src/android_internal/dropbox_service.h"
#include "src/android_internal/incident_service.h"
#include "src/android_internal/lazy_library_loader.h"
#include "src/android_internal/statsd_logging.h"

namespace perfetto {

void PerfettoCmd::SaveTraceIntoDropboxAndIncidentOrCrash() {
  PERFETTO_CHECK(!dropbox_tag_.empty());

  bool use_dropbox = !trace_config_->incident_report_config().skip_dropbox();
  bool use_incident =
      !trace_config_->incident_report_config().destination_package().empty();

  if (bytes_written_ == 0) {
    LogUploadEvent(PerfettoStatsdAtom::kNotUploadingEmptyTrace);
    if (use_dropbox)
      PERFETTO_LOG("Skipping write to dropbox. Empty trace.");
    if (use_incident)
      PERFETTO_LOG("Skipping write to incident. Empty trace.");
    return;
  }

  // Otherwise, write to Dropbox unless there's a special override in the
  // incident report config.
  if (use_dropbox) {
    SaveOutputToDropboxOrCrash();
  }

  // Optionally save the trace as an incident. This is either in addition to, or
  // instead of, the Dropbox write.
  if (use_incident) {
    SaveOutputToIncidentTraceOrCrash();

    // Ask incidentd to create a report, which will read the file we just
    // wrote.
    const auto& cfg = trace_config_->incident_report_config();
    PERFETTO_LAZY_LOAD(android_internal::StartIncidentReport, incident_fn);
    PERFETTO_CHECK(incident_fn(cfg.destination_package().c_str(),
                               cfg.destination_class().c_str(),
                               cfg.privacy_level()));
  }
}

void PerfettoCmd::SaveOutputToDropboxOrCrash() {
  LogUploadEvent(PerfettoStatsdAtom::kUploadDropboxBegin);

  PERFETTO_CHECK(fseek(*trace_out_stream_, 0, SEEK_SET) == 0);

  // DropBox takes ownership of the file descriptor, so give it a duplicate.
  // Also we need to give it a read-only copy of the fd or will hit a SELinux
  // violation (about system_server ending up with a writable FD to our dir).
  char fdpath[64];
  sprintf(fdpath, "/proc/self/fd/%d", fileno(*trace_out_stream_));
  base::ScopedFile read_only_fd(base::OpenFile(fdpath, O_RDONLY));
  PERFETTO_CHECK(read_only_fd);

  PERFETTO_LAZY_LOAD(android_internal::SaveIntoDropbox, dropbox_fn);
  if (dropbox_fn(dropbox_tag_.c_str(), read_only_fd.release())) {
    LogUploadEvent(PerfettoStatsdAtom::kUploadDropboxSuccess);
    PERFETTO_LOG("Wrote %" PRIu64
                 " bytes (before compression) into DropBox with tag %s",
                 bytes_written_, dropbox_tag_.c_str());
  } else {
    LogUploadEvent(PerfettoStatsdAtom::kUploadDropboxFailure);
    PERFETTO_FATAL("DropBox upload failed");
  }
}

// Open a staging file (unlinking the previous instance), copy the trace
// contents over, then rename to a final hardcoded path (known to incidentd).
// Such tracing sessions should not normally overlap. We do not use unique
// unique filenames to avoid creating an unbounded amount of files in case of
// errors.
void PerfettoCmd::SaveOutputToIncidentTraceOrCrash() {
  LogUploadEvent(PerfettoStatsdAtom::kUploadIncidentBegin);
  char kIncidentTracePath[256];
  sprintf(kIncidentTracePath, "%s/incident-trace", kStateDir);

  char kTempIncidentTracePath[256];
  sprintf(kTempIncidentTracePath, "%s.temp", kIncidentTracePath);

  PERFETTO_CHECK(unlink(kTempIncidentTracePath) == 0 || errno == ENOENT);

  // SELinux constrains the set of readers.
  base::ScopedFile staging_fd =
      base::OpenFile(kTempIncidentTracePath, O_CREAT | O_RDWR, 0666);
  PERFETTO_CHECK(staging_fd);
  off_t offset = 0;
  auto wsize = sendfile(*staging_fd, fileno(*trace_out_stream_), &offset,
                        static_cast<size_t>(bytes_written_));
  PERFETTO_CHECK(wsize == static_cast<ssize_t>(bytes_written_));
  staging_fd.reset();
  PERFETTO_CHECK(rename(kTempIncidentTracePath, kIncidentTracePath) == 0);
  // Note: not calling fsync(2), as we're not interested in the file being
  // consistent in case of a crash.
  LogUploadEvent(PerfettoStatsdAtom::kUploadIncidentSuccess);
}

// static
base::ScopedFile PerfettoCmd::OpenDropboxTmpFile() {
  // If we are tracing to DropBox, there's no need to make a
  // filesystem-visible temporary file.
  auto fd = base::OpenFile(kStateDir, O_TMPFILE | O_RDWR, 0600);
  if (!fd)
    PERFETTO_PLOG("Could not create a temporary trace file in %s", kStateDir);
  return fd;
}

void PerfettoCmd::LogUploadEventAndroid(PerfettoStatsdAtom atom) {
  if (dropbox_tag_.empty())
    return;
  PERFETTO_LAZY_LOAD(android_internal::StatsdLogEvent, log_event_fn);
  base::Uuid uuid(uuid_);
  log_event_fn(atom, uuid.lsb(), uuid.msb());
}

}  // namespace perfetto
