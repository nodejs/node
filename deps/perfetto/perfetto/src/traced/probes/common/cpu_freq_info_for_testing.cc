/*
 * Copyright (C) 2020 The Android Open Source Project
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

#include "src/traced/probes/common/cpu_freq_info_for_testing.h"

#include <dirent.h>
#include <sys/stat.h>

#include <algorithm>
#include <memory>

#include "perfetto/ext/base/file_utils.h"
#include "perfetto/ext/base/scoped_file.h"
#include "perfetto/ext/base/temp_file.h"

namespace perfetto {

namespace {

const char kCpuFrequenciesAndroidLittleCore[] =
    "300000 576000 748800 998400 1209600 1324800 1516800 1612800 1708800 \n";

const char kCpuFrequenciesAndroidBigCore[] =
    "300000 652800 825600 979200 1132800 1363200 1536000 1747200 1843200 "
    "1996800 \n";

}  // namespace

CpuFreqInfoForTesting::CpuFreqInfoForTesting()
    : fake_cpu_dir_(base::TempDir::Create()) {
  // Create a subset of /sys/devices/system/cpu.
  AddDir("cpuidle");
  AddDir("cpu0");
  AddDir("cpu0/cpufreq");
  AddFile("cpu0/cpufreq/scaling_available_frequencies",
          kCpuFrequenciesAndroidLittleCore);
  AddDir("cpufreq");
  AddDir("cpu1");
  AddDir("cpu1/cpufreq");
  AddFile("cpu1/cpufreq/scaling_available_frequencies",
          kCpuFrequenciesAndroidBigCore);
  AddDir("power");
}

CpuFreqInfoForTesting::~CpuFreqInfoForTesting() {
  for (auto path : files_to_remove_)
    RmFile(path);
  std::reverse(dirs_to_remove_.begin(), dirs_to_remove_.end());
  for (auto path : dirs_to_remove_)
    RmDir(path);
}

std::unique_ptr<CpuFreqInfo> CpuFreqInfoForTesting::GetInstance() {
  return std::unique_ptr<CpuFreqInfo>(new CpuFreqInfo(fake_cpu_dir_.path()));
}

void CpuFreqInfoForTesting::AddDir(std::string path) {
  dirs_to_remove_.push_back(path);
  mkdir(AbsolutePath(path).c_str(), 0755);
}

void CpuFreqInfoForTesting::AddFile(std::string path, std::string content) {
  files_to_remove_.push_back(path);
  base::ScopedFile fd(
      base::OpenFile(AbsolutePath(path), O_WRONLY | O_CREAT | O_TRUNC, 0600));
  PERFETTO_CHECK(base::WriteAll(fd.get(), content.c_str(), content.size()) ==
                 static_cast<ssize_t>(content.size()));
}

void CpuFreqInfoForTesting::RmDir(std::string path) {
  PERFETTO_CHECK(rmdir(AbsolutePath(path).c_str()) == 0);
}

void CpuFreqInfoForTesting::RmFile(std::string path) {
  PERFETTO_CHECK(remove(AbsolutePath(path).c_str()) == 0);
}

std::string CpuFreqInfoForTesting::AbsolutePath(std::string path) {
  return fake_cpu_dir_.path() + "/" + path;
}

}  // namespace perfetto
