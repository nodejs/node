// Copyright (C) 2019 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <random>
#include <set>
#include <unordered_set>

#include <benchmark/benchmark.h>

#include "perfetto/base/logging.h"
#include "perfetto/ext/base/utils.h"
#include "src/base/test/utils.h"
#include "src/traced/probes/ftrace/kallsyms/kernel_symbol_map.h"

namespace {

bool IsBenchmarkFunctionalOnly() {
  return getenv("BENCHMARK_FUNCTIONAL_TEST_ONLY") != nullptr;
}

void BenchmarkArgs(benchmark::internal::Benchmark* b) {
  if (IsBenchmarkFunctionalOnly()) {
    b->Ranges({{16, 16}, {16, 16}});
  } else {
    b->RangeMultiplier(2)->Ranges({{4, 512}, {4, 512}});
  }
}

struct ExpectedSym {
  uint64_t addr;
  const char* name;
};

// This set of symbols has been chosen by randomly picking 40 random symbols
// from the original kallsyms.
ExpectedSym kExpectedSyms[] = {
    {0xffffff8f79c0d978, "__map_memblock"},
    {0xffffff8f78fddbb8, "smack_inode_getsecid"},
    {0xffffff8f78fe43b4, "msm_smmu_set_attribute"},
    {0xffffff8f79d23e20, "__initcall_41_dm_verity_init6"},
    {0xffffff8f74206c5c, "sme_update_fast_transition_enabled"},
    {0xffffff8f74878c8c, "tavil_hph_idle_detect_put"},
    {0xffffff8f78fd7db0, "privileged_wrt_inode_uidgid"},
    {0xffffff8f78ffe030, "__hrtimer_tasklet_trampoline"},
    {0xffffff8f78fd86b0, "store_enable"},
    {0xffffff8f78ffbcb8, "raw6_exit_net"},
    {0xffffff8f78ffa6ec, "idProduct_show"},
    {0xffffff8f78fd99c0, "perf_tp_event"},
    {0xffffff8f78fe1468, "rpmh_tx_done"},
    {0xffffff8f78fda274, "page_unlock_anon_vma_read"},
    {0xffffff8f78ffedfc, "vmstat_period_ms_operations_open"},
    {0xffffff8f78fe0148, "devm_gpio_request"},
    {0xffffff8f77915028, "ctx_sched_out"},
    {0xffffff8f77ccdc2c, "gcm_hash_crypt_remain_continue"},
    {0xffffff8f790022ec, "loop_init"},
    {0xffffff8f78ff0004, "pcim_release"},
    {0xffffff8f78fe1d8c, "uart_close"},
    {0xffffff8f78fda9d4, "pipe_lock"},
    {0xffffff8f78e62c68, "local_bh_enable.117091"},
    {0xffffff8f78fd918c, "fork_idle"},
    {0xffffff8f78fe24c4, "drm_dp_downstream_debug"},
    {0xffffff8f78ff41d0, "inet_addr_onlink"},
    {0xffffff8f78fdf2d4, "idr_alloc"},
    {0xffffff8f78ff073c, "fts_remove"},
    {0xffffff8f78ffe294, "xfrm4_local_error"},
    {0xffffff8f79001994, "cpu_feature_match_PMULL_init"},
    {0xffffff8f78ff4740, "xfrm_state_find"},
    {0xffffff8f78ff58b0, "inet_del_offload"},
    {0xffffff8f742041ac, "csr_is_conn_state_connected_infra"},
    {0xffffff8f78fe1fd4, "diag_add_client"},
    {0xffffff8f78ffc000, "trace_raw_output_mm_vmscan_kswapd_sleep"},
    {0xffffff8f78fe6388, "scsi_queue_insert"},
    {0xffffff8f78fdd480, "selinux_sb_clone_mnt_opts"},
    {0xffffff8f78fe0e9c, "clk_fixed_rate_recalc_rate"},
    {0xffffff8f78fedaec, "cap_inode_killpriv"},
    {0xffffff8f79002b64, "audio_amrwb_init"},
};

}  // namespace

static void BM_KallSyms(benchmark::State& state) {
  perfetto::KernelSymbolMap::kTokenIndexSampling =
      static_cast<size_t>(state.range(0));
  perfetto::KernelSymbolMap::kSymIndexSampling =
      static_cast<size_t>(state.range(1));
  perfetto::KernelSymbolMap kallsyms;

  // Don't run the benchmark on the CI as it requires pushing all test data,
  // which slows down significantly the CI.
  const bool skip = IsBenchmarkFunctionalOnly();
  if (!skip) {
    kallsyms.Parse(perfetto::base::GetTestDataPath("test/data/kallsyms.txt"));
  }

  for (auto _ : state) {
    for (size_t i = 0; i < perfetto::base::ArraySize(kExpectedSyms); i++) {
      const auto& exp = kExpectedSyms[i];
      PERFETTO_CHECK(skip || kallsyms.Lookup(exp.addr) == exp.name);
    }
  }

  state.counters["mem"] = kallsyms.size_bytes();
}

BENCHMARK(BM_KallSyms)->Apply(BenchmarkArgs);
