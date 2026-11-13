// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// "redline selftest": end-to-end checks of the execution layer against a
// real target. Modeled on Fuzzilli's startup tests, extended with the
// behaviours the fuzzer relies on (timeouts, respawning, output capture,
// coverage evaluation and edge determinism). Prints one PASS/FAIL line per
// check and exits non-zero if any check failed.

#include <dirent.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <map>
#include <set>
#include <thread>

#include "experimental/redline/base/file_util.h"
#include "experimental/redline/base/flags.h"
#include "experimental/redline/base/log.h"
#include "experimental/redline/base/time_util.h"
#include "experimental/redline/commands/command.h"
#include "experimental/redline/commands/flags_command.h"
#include "experimental/redline/commands/selftest_checker.h"
#include "experimental/redline/commands/selftest_selection.h"
#include "experimental/redline/core/environment.h"
#include "experimental/redline/core/program.h"
#include "experimental/redline/core/random.h"
#include "experimental/redline/core/tunable.h"
#include "experimental/redline/corpus/corpus.h"
#include "experimental/redline/corpus/feature_weights.h"
#include "experimental/redline/engine/amplifier.h"
#include "experimental/redline/engine/config.h"
#include "experimental/redline/engine/crash.h"
#include "experimental/redline/engine/d8_flags.h"
#include "experimental/redline/engine/flag_minimizer.h"
#include "experimental/redline/engine/fuzzer.h"
#include "experimental/redline/engine/prelude.h"
#include "experimental/redline/engine/shared_coverage.h"
#include "experimental/redline/exec/coverage.h"
#include "experimental/redline/exec/process_run.h"
#include "experimental/redline/exec/runner.h"
#include "experimental/redline/fast/ast.h"
#include "experimental/redline/fast/parser.h"
#include "experimental/redline/fast/printer.h"
#include "experimental/redline/minimizer/minimizer.h"

namespace redline {

namespace {

constexpr const char* kLog = "selftest";

std::string Describe(const Execution& e) {
  return StringPrintf("%s (exit code %d, signal %d, %.1f ms)",
                      OutcomeName(e.outcome), e.exit_code, e.signal,
                      static_cast<double>(e.exec_time_us) / 1000.0);
}

// Runs `script` and returns false on infrastructure failure (reported as
// a failed check).
bool RunOrFail(Checker* checker, ScriptRunner* runner, const char* name,
               const std::string& script, ScriptRunner::Result* result) {
  std::string error;
  if (!runner->Run(script, result, &error)) {
    checker->Check(name, false, "execution failed: " + error);
    return false;
  }
  return true;
}

// experimental/redline/testdata, found from the location of the binary, or
// empty. Look for testdata/selection so we do not pick up unrelated
// out/<dir>/testdata directories created by other build targets.
std::string DefaultTestdataDir() {
  std::string selection = RedlineDataDir("testdata/selection");
  if (!selection.empty()) {
    return selection.substr(0, selection.rfind('/'));
  }
  return RedlineDataDir("testdata");
}

// Offline: the profiles of the random d8 flags. The Turbolev profile must not
// draw the flags that only Maglev reads, and the Maglev profile must not draw
// the Turbolev and Turboshaft flags.
void RunD8FlagProfileChecks(Checker* checker) {
  // Returns the first flag of 500 draws that starts with one of `prefixes`
  // and is not in `allowed`.
  auto draws_any = [](D8FlagProfile profile,
                      std::initializer_list<const char*> prefixes,
                      std::initializer_list<const char*> allowed = {}) {
    Random rng(7);
    std::string found;
    for (int i = 0; i < 500 && found.empty(); i++) {
      for (const std::string& flag : RandomD8Flags(rng, profile)) {
        if (std::any_of(allowed.begin(), allowed.end(),
                        [&](const char* name) { return flag == name; })) {
          continue;
        }
        for (const char* prefix : prefixes) {
          if (flag.rfind(prefix, 0) == 0) {
            found = flag;
          }
        }
      }
    }
    return found;
  };
  D8FlagProfile parsed = D8FlagProfile::kMaglev;
  checker->Check("random flags: the profile names parse",
                 ParseD8FlagProfile("turbolev", &parsed) &&
                     parsed == D8FlagProfile::kTurbolev &&
                     ParseD8FlagProfile("maglev", &parsed) &&
                     parsed == D8FlagProfile::kMaglev &&
                     !ParseD8FlagProfile("turbofan", &parsed),
                 "");
  // Turbolev builds its graph with the Maglev graph builder, so it also reads
  // the Maglev assertions and the allowed --no-maglev- flags. Only Turbolev
  // reads --maglev-range-analysis.
  std::string maglev_only = draws_any(
      D8FlagProfile::kTurbolev,
      {"--maglev-future", "--maglev-non-eager", "--max-maglev",
       "--optimize-on-next-call-optimizes-to-maglev", "--no-maglev-",
       "--maglev-as-top-tier", "--maglev-inline-api-calls"},
      {"--no-maglev-inlining", "--no-maglev-poly-calls",
       "--no-maglev-escape-analysis", "--no-maglev-optimistic-peeled-loops",
       "--no-maglev-truncation", "--no-maglev-cse",
       "--no-maglev-range-analysis"});
  checker->Check("random flags: the turbolev profile draws no Maglev-only flag",
                 maglev_only.empty(), maglev_only);
  const bool turbolev_has_maglev_assert =
      !draws_any(D8FlagProfile::kTurbolev, {"--maglev-assert"},
                 {"--maglev-assert-types"})
           .empty() &&
      !draws_any(D8FlagProfile::kTurbolev, {"--maglev-assert-types"}).empty();
  checker->Check("random flags: the turbolev profile draws Maglev assertions",
                 turbolev_has_maglev_assert, "");
  std::string turbolev_only = draws_any(
      D8FlagProfile::kMaglev,
      {"--no-turbolev", "--no-turboshaft", "--turboshaft-",
       "--verify-turboshaft", "--maglev-range-verification",
       "--always-osr-from-maglev", "--no-additive-safe-int-feedback",
       "--max-inlined-bytecode-size", "--no-maglev-range-analysis",
       "--no-turbo-", "--turbo-", "--optimize-maglev-optimizes-to-turbofan",
       "--stress-concurrent-inlining", "--concurrent-recompilation-"});
  checker->Check("random flags: the maglev profile draws no Turbolev flag",
                 turbolev_only.empty(), turbolev_only);

  // The rules of the draw. A draw names each flag at most once. A flag that
  // needs another flag comes only with that flag. The cumulative inlining
  // limit is not smaller than the limit for one function.
  auto name_of = [](const std::string& flag) {
    return flag.substr(0, flag.find('='));
  };
  auto value_of = [](const std::string& flag) -> int64_t {
    size_t eq = flag.find('=');
    return eq == std::string::npos
               ? -1
               : std::strtoll(flag.c_str() + eq + 1, nullptr, 10);
  };
  std::string bad_draw;
  for (D8FlagProfile profile :
       {D8FlagProfile::kMaglev, D8FlagProfile::kTurbolev}) {
    Random rng(11);
    for (int i = 0; i < 500 && bad_draw.empty(); i++) {
      std::vector<std::string> flags = RandomD8Flags(rng, profile);
      std::set<std::string> names;
      int64_t size = -1;
      bool chaos = false;
      for (const std::string& flag : flags) {
        const std::string name = name_of(flag);
        if (!names.insert(name).second) {
          bad_draw = "two " + name;
        } else if (name == "--max-inlined-bytecode-size") {
          size = value_of(flag);
        } else if (name == "--max-inlined-bytecode-size-cumulative" &&
                   (size < 0 || value_of(flag) < size)) {
          bad_draw = flag + " without a smaller --max-inlined-bytecode-size";
        } else if (name == "--scavenger-chaos-mode") {
          chaos = true;
        } else if (name == "--scavenger-chaos-mode-threshold" && !chaos) {
          bad_draw = flag + " without --scavenger-chaos-mode";
        }
      }
      if (bad_draw.empty()) {
        if (names.count("--verify-bytecode-full") > 0 &&
            names.count("--verify-bytecode-light") > 0) {
          bad_draw = "both --verify-bytecode-full and --verify-bytecode-light";
        } else if (names.count("--no-sparkplug") > 0 &&
                   names.count("--always-sparkplug") > 0) {
          bad_draw = "both --no-sparkplug and --always-sparkplug";
        } else if (names.count("--assert-types") > 0 &&
                   (names.count("--stress-concurrent-inlining") > 0 ||
                    names.count("--stress-concurrent-inlining-attach-code") >
                        0)) {
          bad_draw = "--assert-types with --stress-concurrent-inlining";
        }
      }
    }
  }
  checker->Check(
      "random flags: a draw names each flag once, with the flags it needs",
      bad_draw.empty(), bad_draw);
  // --single-threaded and --assert-types stop the extra jobs of
  // --stress-concurrent-inlining.
  TunableRegistry& registry = TunableRegistry::Instance();
  std::string error;
  bool set =
      registry.Set("flags.single_threaded", 1, &error) &&
      registry.Set("flags.stress_concurrent_inlining", 1, &error) &&
      registry.Set("flags.stress_concurrent_inlining_attach_code", 1, &error);
  std::string concurrent = set ? draws_any(D8FlagProfile::kTurbolev,
                                           {"--stress-concurrent-inlining"})
                               : error;
  registry.Reset();
  checker->Check(
      "random flags: --single-threaded comes without "
      "--stress-concurrent-inlining",
      set && concurrent.empty(), concurrent);
  bool set_assert =
      registry.Set("flags.assert_types", 1, &error) &&
      registry.Set("flags.stress_concurrent_inlining", 1, &error) &&
      registry.Set("flags.stress_concurrent_inlining_attach_code", 1, &error);
  std::string assert_concurrent =
      set_assert ? draws_any(D8FlagProfile::kTurbolev,
                             {"--stress-concurrent-inlining"})
                 : error;
  registry.Reset();
  checker->Check(
      "random flags: --assert-types comes without "
      "--stress-concurrent-inlining",
      set_assert && assert_concurrent.empty(), assert_concurrent);

  // A fixed --single-threaded or --assert-types also removes the
  // --stress-concurrent-inlining flags. Without a fixed flag, the same
  // tunables give these flags in each draw.
  auto fixed_draws = [](const std::vector<std::string>& fixed) {
    Random rng(19);
    int with_concurrent = 0;
    for (int i = 0; i < 100; i++) {
      for (const std::string& flag :
           RandomD8Flags(rng, D8FlagProfile::kTurbolev, fixed)) {
        if (flag.rfind("--stress-concurrent-inlining", 0) == 0) {
          with_concurrent++;
          break;
        }
      }
    }
    return with_concurrent;
  };
  const bool set_concurrent =
      registry.Set("flags.stress_concurrent_inlining", 1, &error) &&
      registry.Set("flags.stress_concurrent_inlining_attach_code", 1, &error);
  const int no_fixed = fixed_draws({});
  const int fixed_st = fixed_draws({"--single-threaded"});
  const int fixed_assert = fixed_draws({"--assert-types"});
  registry.Reset();
  checker->Check(
      "random flags: a fixed --single-threaded or --assert-types removes "
      "--stress-concurrent-inlining",
      set_concurrent && no_fixed == 100 && fixed_st == 0 && fixed_assert == 0,
      StringPrintf("no fixed flag %d, --single-threaded %d, --assert-types %d",
                   no_fixed, fixed_st, fixed_assert));

  // --maglev-as-top-tier turns off Turbofan and Turbolev, and turns on Maglev.
  // Like Fuzzilli, the Maglev profile draws it only with a fixed
  // --no-turbofan. A fixed or drawn --turbofan, --turbolev or --no-maglev
  // removes it.
  auto top_tier_draws = [](const std::vector<std::string>& fixed) {
    Random rng(23);
    int with_top_tier = 0;
    for (int i = 0; i < 100; i++) {
      const std::vector<std::string> flags =
          RandomD8Flags(rng, D8FlagProfile::kMaglev, fixed);
      if (std::find(flags.begin(), flags.end(), "--maglev-as-top-tier") !=
          flags.end()) {
        with_top_tier++;
      }
    }
    return with_top_tier;
  };
  auto sanitized_has_top_tier = [](const char* other) {
    std::vector<std::string> flags = {"--maglev-as-top-tier", other};
    SanitizeD8Flags(&flags);
    return std::find(flags.begin(), flags.end(), "--maglev-as-top-tier") !=
           flags.end();
  };
  const bool set_top_tier = registry.Set("flags.maglev_as_top_tier", 1, &error);
  const int top_no_fixed = top_tier_draws({});
  const int top_no_turbofan = top_tier_draws({"--no-turbofan"});
  const int top_turbolev = top_tier_draws({"--no-turbofan", "--turbolev"});
  const int top_no_maglev = top_tier_draws({"--no-turbofan", "--no-maglev"});
  const int top_disabled =
      top_tier_draws({"--no-turbofan", "--disable-optimizing-compilers"});
  registry.Reset();
  const bool drawn_removed = !sanitized_has_top_tier("--turbofan") &&
                             !sanitized_has_top_tier("--turbolev") &&
                             !sanitized_has_top_tier("--no-maglev") &&
                             sanitized_has_top_tier("--no-turbofan");
  checker->Check(
      "random flags: --maglev-as-top-tier needs --no-turbofan and no clash",
      set_top_tier && top_no_fixed == 0 && top_no_turbofan == 100 &&
          top_turbolev == 0 && top_no_maglev == 0 && top_disabled == 0 &&
          drawn_removed,
      StringPrintf("no fixed flag %d, --no-turbofan %d, --turbolev %d, "
                   "--no-maglev %d, --disable-optimizing-compilers %d, "
                   "drawn %d",
                   top_no_fixed, top_no_turbofan, top_turbolev, top_no_maglev,
                   top_disabled, drawn_removed ? 1 : 0));

  // `redline flags` draws reproducible flag sets and stops when random `d8`
  // flags are off.
  auto run_flags = [](std::initializer_list<const char*> args,
                      std::vector<std::string>* out, std::string* err) {
    std::vector<const char*> argv = {"redline", "flags"};
    argv.insert(argv.end(), args.begin(), args.end());
    Flags parsed_flags =
        Flags::Parse(static_cast<int>(argv.size()), argv.data(), 2);
    return DrawD8FlagLines(parsed_flags, out, err);
  };
  std::vector<std::string> maglev_lines, maglev_again, turbolev_lines, bad_out;
  std::string flags_err, bad_err, disabled_err;
  const bool maglev_ok = run_flags({"--preset=maglev", "--n=20", "--seed=7"},
                                   &maglev_lines, &flags_err) &&
                         run_flags({"--preset=maglev", "--n=20", "--seed=7"},
                                   &maglev_again, &flags_err);
  const bool turbolev_ok = run_flags(
      {"--preset=turbolev", "--n=20", "--seed=7"}, &turbolev_lines, &flags_err);
  const bool bad_rejected =
      !run_flags({"--preset=maglev", "--n=0"}, &bad_out, &bad_err);
  const bool disabled_rejected =
      !run_flags({"--preset=default"}, &bad_out, &disabled_err) &&
      disabled_err.find("randomize_d8_flags") != std::string::npos;
  registry.Reset();
  const bool saw_top_tier = std::any_of(
      maglev_lines.begin(), maglev_lines.end(), [](const std::string& line) {
        return line.find("--maglev-as-top-tier") != std::string::npos;
      });
  const bool saw_turbolev_flag = std::any_of(
      turbolev_lines.begin(), turbolev_lines.end(),
      [](const std::string& line) {
        return line.find("--verify-turboshaft") != std::string::npos ||
               line.find("--stress-concurrent-inlining") != std::string::npos;
      });
  checker->Check(
      "random flags: the flags command draws preset flag sets",
      maglev_ok && turbolev_ok && bad_rejected && disabled_rejected &&
          maglev_lines.size() == 20 && turbolev_lines.size() == 20 &&
          maglev_lines == maglev_again && maglev_lines != turbolev_lines &&
          saw_top_tier && saw_turbolev_flag,
      flags_err);
}

// Offline: crash signatures. A recursion that overflows the stack must get
// the same signature wherever the stack ran out; other crashes keep their
// first frames.
void RunCrashSignatureChecks(Checker* checker) {
  auto frame = [](const char* mangled) {
    return StringPrintf("./d8(%s+0x10)[0x5555]\n", mangled);
  };
  auto trace = [&](const char* top, int depth) {
    std::string text = "Received signal 11 SEGV_MAPERR 7ffd00000ff8\n\n";
    text += "==== C stack trace ===============================\n\n";
    text += frame(top);
    const char* cycle[] = {"_ZN2v88internal3FooEv", "_ZN2v88internal3BarEv",
                           "_ZN2v88internal3BazEv"};
    for (int i = 0; i < depth; i++) {
      text += frame(cycle[i % 3]);
    }
    return text + "[end of stack trace]\n";
  };
  CrashSignature a =
      CrashSignature::FromOutput(11, 0, trace("_ZN2v88internal4Top1Ev", 57));
  CrashSignature b =
      CrashSignature::FromOutput(11, 0, trace("_ZN2v88internal4Top2Ev", 55));
  checker->Check(
      "crash signature: a stack overflow does not depend on the faulting frame",
      a == b, a.text + " vs " + b.text);
  checker->Check("crash signature: a stack overflow names its recursion",
                 a.text ==
                     "sig11 SEGV_MAPERR stack-overflow v8::internal::Bar | "
                     "v8::internal::Baz | v8::internal::Foo",
                 a.text);
  CrashSignature c =
      CrashSignature::FromOutput(11, 0, trace("_ZN2v88internal4Top1Ev", 2));
  checker->Check("crash signature: other crashes keep their first three frames",
                 c.text ==
                     "sig11 SEGV_MAPERR v8::internal::Top1 < v8::internal::Foo "
                     "< v8::internal::Bar",
                 c.text);
  CrashSignature d = CrashSignature::FromOutput(
      5, 0,
      "abort: CSA_DCHECK failed: Torque assert 'Is<A>(o)' failed "
      "[src/builtins/cast.tq:1025] [gen/torque-generated/src/builtins/"
      "regexp-test-tq-csa.cc:174]\n\nStacktrace:\n");
  checker->Check(
      "crash signature: a failed CSA_DCHECK names its builtin and message",
      d.text == "csa regexp-test-tq-csa.cc:174 Torque assert 'Is<A>(o)' failed",
      d.text);
}

// Offline: the d8 bundle of a state-dependent crash (MakeReprlBundle).
void RunReprlBundleChecks(Checker* checker) {
  std::string bundle;
  bool ok = MakeReprlBundle("// header\n//\n", {"let x = 1;", "let x = 2;\n"},
                            &bundle);
  checker->Check("bundle: one script part for each script, in order",
                 ok && bundle ==
                           "// header\n//\n"
                           "// JS_BUNDLE_SCRIPT\nlet x = 1;\n"
                           "// JS_BUNDLE_SCRIPT\nlet x = 2;\n",
                 bundle);
  checker->Check(
      "bundle: a script with the marker prefix is refused",
      !MakeReprlBundle("", {"s = '// JS_BUNDLE_MODULE:a.mjs';"}, &bundle), "");
  checker->Check("bundle: a header line that is not a comment is refused",
                 !MakeReprlBundle("// a\nvar b;\n", {"1;"}, &bundle), "");
}

// Offline: the coverage header (experimental/redline/d8/cov.h) on synthetic
// shared memory.
void RunCoverageHeaderChecks(Checker* checker) {
  std::string error;
  std::unique_ptr<CoverageMap> map = CoverageMap::Create(9998, &error);
  if (!checker->Check("coverage header: create a coverage map", map != nullptr,
                      error)) {
    return;
  }
  int fd = shm_open(map->shm_name().c_str(), O_RDWR, 0);
  void* mapping = fd < 0 ? MAP_FAILED
                         : mmap(nullptr, CoverageMap::kShmSize,
                                PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (fd >= 0) {
    close(fd);
  }
  const bool mapped = mapping != MAP_FAILED;
  if (!checker->Check("coverage header: map the shared memory", mapped,
                      mapped ? "" : strerror(errno))) {
    return;
  }
  uint8_t* shm = static_cast<uint8_t*>(mapping);
  // A target with inline-bool-flag, 80 code edges and 10 builtins blocks
  // writes this header.
  cov::Header header = {};
  header.magic = cov::kMagic;
  header.version = cov::kVersion;
  header.header_size = cov::kHeaderSize;
  header.shm_size = static_cast<uint32_t>(CoverageMap::kShmSize);
  header.mode = static_cast<uint32_t>(cov::Mode::kInlineBoolFlag);
  header.bits_per_edge = 1;
  header.num_edges = 90;
  header.max_edges = cov::MaxEdges(header.shm_size, 1);
  header.num_regions = 2;
  header.regions[0] = {static_cast<uint32_t>(cov::RegionKind::kCode), 1, 80};
  header.regions[1] = {static_cast<uint32_t>(cov::RegionKind::kBuiltins), 81,
                       10};
  header.build_id_size = 2;
  header.build_id[0] = 0xab;
  header.build_id[1] = 0x01;
  memcpy(shm, &header, sizeof(header));
  bool ok = map->FinishInitialization(&error);
  checker->Check("coverage header: read a valid header", ok, error);
  if (ok) {
    checker->Check(
        "coverage header: geometry",
        map->num_edges() == 90 &&
            strcmp(map->mode_name(), "inline-bool-flag") == 0 &&
            map->build_id() == "ab01",
        StringPrintf("%u edges, mode %s, build id %s", map->num_edges(),
                     map->mode_name(), map->build_id().c_str()));
    // The bitmap starts after the header.
    uint8_t* bits = shm + cov::kHeaderSize;
    bits[5 / 8] |= static_cast<uint8_t>(1u << (5 % 8));
    bits[85 / 8] |= static_cast<uint8_t>(1u << (85 % 8));
    const std::vector<uint32_t> expected = {5, 85};
    checker->Check("coverage header: bits after the header",
                   map->CurrentEdges() == expected,
                   StringPrintf("%zu edges", map->CurrentEdges().size()));
    // A new target process with other edges is another binary.
    cov::Header other = header;
    other.num_edges = 91;
    other.regions[1].count = 11;
    memcpy(shm, &other, sizeof(other));
    std::string changed;
    checker->Check("coverage header: a changed header is an error",
                   !map->FinishInitialization(&changed) &&
                       changed.find("mixing binaries") != std::string::npos,
                   changed);
  }
  // Headers that this Redline rejects.
  struct BadHeader {
    const char* name;
    cov::Header header;
  };
  BadHeader bad[4] = {{"no magic", header},
                      {"a region beyond the edges", header},
                      {"no instrumentation", header},
                      {"a wrong edge size", header}};
  bad[0].header.magic = 0;
  bad[1].header.regions[1].count = 11;
  bad[2].header.mode = static_cast<uint32_t>(cov::Mode::kNone);
  bad[3].header.mode = static_cast<uint32_t>(cov::Mode::kInline8bitCounters);
  for (const BadHeader& b : bad) {
    std::unique_ptr<CoverageMap> fresh = CoverageMap::Create(9997, &error);
    if (fresh == nullptr) {
      checker->Check("coverage header: create a coverage map", false, error);
      break;
    }
    int bad_fd = shm_open(fresh->shm_name().c_str(), O_RDWR, 0);
    if (bad_fd >= 0) {
      if (pwrite(bad_fd, &b.header, sizeof(b.header), 0) !=
          static_cast<ssize_t>(sizeof(b.header))) {
        bad_fd = -1;
      }
      close(bad_fd);
    }
    std::string reason;
    checker->Check(StringPrintf("coverage header: reject %s", b.name).c_str(),
                   bad_fd >= 0 && !fresh->FinishInitialization(&reason),
                   reason);
  }
  munmap(mapping, CoverageMap::kShmSize);
}

// Offline: the counters layout of the coverage (exec/coverage.h) on a
// synthetic header, and the feature weights (corpus/feature_weights.h).
void RunCoverageCounterChecks(Checker* checker) {
  const uint32_t buckets[][2] = {{1, 0},   {2, 1},   {3, 2},  {4, 3},  {7, 3},
                                 {8, 4},   {15, 4},  {16, 5}, {31, 5}, {32, 6},
                                 {127, 6}, {128, 7}, {255, 7}};
  bool buckets_ok = true;
  for (const auto& [count, bucket] : buckets) {
    buckets_ok = buckets_ok &&
                 CoverageMap::Bucket(static_cast<uint8_t>(count)) == bucket;
  }
  checker->Check("coverage counters: AFL bucket table", buckets_ok, "");

  std::string error;
  std::unique_ptr<CoverageMap> map = CoverageMap::Create(9999, &error);
  if (!checker->Check("coverage counters: create a coverage map",
                      map != nullptr, error)) {
    return;
  }
  // A target with 80 code edges, 10 builtins blocks and 10 optimized code
  // features writes this header.
  int fd = shm_open(map->shm_name().c_str(), O_RDWR, 0);
  void* mapping = fd < 0 ? MAP_FAILED
                         : mmap(nullptr, CoverageMap::kShmSize,
                                PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (fd >= 0) {
    close(fd);
  }
  const bool mapped = mapping != MAP_FAILED;
  if (!checker->Check("coverage counters: map the shared memory", mapped,
                      mapped ? "" : strerror(errno))) {
    return;
  }
  uint8_t* shm = static_cast<uint8_t*>(mapping);
  cov::Header header = {};
  header.magic = cov::kMagic;
  header.version = cov::kVersion;
  header.header_size = cov::kHeaderSize;
  header.shm_size = static_cast<uint32_t>(CoverageMap::kShmSize);
  header.mode = static_cast<uint32_t>(cov::Mode::kInline8bitCounters);
  header.bits_per_edge = 8;
  header.num_edges = 100;
  header.max_edges = cov::MaxEdges(header.shm_size, 8);
  header.num_regions = 3;
  header.regions[0] = {static_cast<uint32_t>(cov::RegionKind::kCode), 1, 80};
  header.regions[1] = {static_cast<uint32_t>(cov::RegionKind::kBuiltins), 81,
                       10};
  header.regions[2] = {static_cast<uint32_t>(cov::RegionKind::kOptimized), 91,
                       10};
  memcpy(shm, &header, sizeof(header));
  CoverageMap::Watch watch;
  watch.edges = {7, 5};
  map->SetWatch(watch);
  bool ok = map->FinishInitialization(&error);
  checker->Check("coverage counters: read the header", ok, error);
  if (ok) {
    checker->Check(
        "coverage counters: geometry",
        map->counters_mode() && map->real_edges() == 100 &&
            map->counted_edges() == 90 && map->watched().size() == 12 &&
            map->num_edges() == 100 + 12 * CoverageMap::kBuckets,
        StringPrintf("features %u, counted %u, watched %zu", map->num_edges(),
                     map->counted_edges(), map->watched().size()));
    uint8_t* counters = shm + cov::kHeaderSize;
    counters[5] = 3;
    counters[50] = 1;
    counters[91] = 200;
    map->Refresh();
    // Edge 5 is watched edge 0 (bucket 2), edge 91 is watched edge 2
    // (bucket 7). Bucket features start at 101.
    const std::vector<uint32_t> expected = {5, 50, 91, 103, 124};
    checker->Check(
        "coverage counters: hit bits and bucket features",
        map->CurrentEdges() == expected && map->CountCurrentEdges() == 2,
        StringPrintf("%zu features, %u counted edges",
                     map->CurrentEdges().size(), map->CountCurrentEdges()));
    // Several counters in one 8-byte word. Also a watched edge in the middle
    // of the optimized region: watched edge 10, bucket 4 for a count of 9.
    map->ClearBitmap();
    counters[8] = 1;
    counters[12] = 255;
    counters[15] = 128;
    counters[99] = 9;
    map->Refresh();
    const std::vector<uint32_t> packed = {8, 12, 15, 99, 185};
    checker->Check("coverage counters: packed hit bits",
                   map->CurrentEdges() == packed,
                   StringPrintf("%zu features", map->CurrentEdges().size()));
    // Crash deduplication counts only the edges. Edge 100 and the bucket
    // features of edge 5 (101 to 108) are in the same 8-byte word.
    SharedCoverage shared;
    const bool shared_ok =
        shared.Initialize(map->num_edges(), map->bitmap_size(), &error,
                          map->counted_edges(), map->real_edges());
    map->ResetState();
    std::string crash_results;
    auto crash = [&](std::initializer_list<std::pair<uint32_t, uint8_t>> hits) {
      map->ClearBitmap();
      for (const auto& [edge, count] : hits) {
        counters[edge] = count;
      }
      map->Refresh();
      const bool local = map->EvaluateCrash();
      const bool session = shared.ClaimNewCrashEdges(map->bitmap());
      crash_results += StringPrintf("%s%d%d", crash_results.empty() ? "" : " ",
                                    local, session);
    };
    // A new edge, the same edge with a different hit count, a new edge.
    crash({{5, 1}});
    crash({{5, 2}});
    crash({{5, 2}, {100, 1}});
    checker->Check("coverage counters: crash edges ignore bucket features",
                   shared_ok && crash_results == "11 00 11", crash_results);
    // A flaky feature that is not a counted edge (an optimized code feature
    // or a bucket feature) does not change coverage_fraction(). The packed
    // bitmap has 3 counted edges: 8, 12 and 15.
    SharedCoverage flaky;
    const bool flaky_ok =
        flaky.Initialize(map->num_edges(), map->bitmap_size(), &error,
                         map->counted_edges(), map->real_edges());
    map->ClearBitmap();
    counters[8] = 1;
    counters[12] = 255;
    counters[15] = 128;
    counters[99] = 9;
    map->Refresh();
    const bool claimed_packed = flaky.ClaimNewEdges(map->bitmap()) == packed;
    const double denominator = static_cast<double>(map->counted_edges() + 1);
    flaky.RecordFlaky(99, 1);
    flaky.RecordFlaky(185, 1);
    const double three_edges = flaky.coverage_fraction();
    flaky.RecordFlaky(12, 1);
    const double two_edges = flaky.coverage_fraction();
    checker->Check(
        "coverage counters: only flaky counted edges reduce the coverage",
        flaky_ok && claimed_packed && flaky.found_edges() == 3 &&
            flaky.suppressed_flaky_edges() == 1 &&
            three_edges == 3 / denominator && two_edges == 2 / denominator,
        StringPrintf(
            "found %u, suppressed %llu, coverage %g then %g edges",
            flaky.found_edges(),
            static_cast<unsigned long long>(flaky.suppressed_flaky_edges()),
            three_edges * denominator, two_edges * denominator));
    map->ClearBitmap();
    counters[5] = 3;
    counters[50] = 1;
    counters[91] = 200;
    map->Refresh();
    FeatureWeights::Params params;
    std::unique_ptr<FeatureWeights> weights =
        FeatureWeights::Build(*map, params, {{5, 1.0}, {50, 0.3}});
    auto near = [](double a, double b) { return std::fabs(a - b) < 1e-6; };
    checker->Check(
        "coverage counters: feature weights",
        near(weights->Of(5), 21.0) && near(weights->Of(50), 7.0) &&
            near(weights->Of(85), 0.5) && near(weights->Of(91), 5.0) &&
            near(weights->Of(101), 0.0) && near(weights->Of(103), 10.5) &&
            near(weights->Of(124), 2.5) && near(weights->Of(10), 1.0) &&
            weights->target_edges() == std::vector<uint32_t>{5, 50},
        StringPrintf("%g %g %g %g %g %g %g", weights->Of(5), weights->Of(50),
                     weights->Of(85), weights->Of(91), weights->Of(101),
                     weights->Of(103), weights->Of(124)));
    // A focus on code edges 40 to 60. The focus edges, the target edges and
    // the buckets of the targets are in the focus. Optimized code features are
    // excluded by default and included when optimized_in_focus is true.
    std::unique_ptr<FeatureWeights> focused = FeatureWeights::Build(
        *map, params, {{5, 1.0}}, FeatureWeights::EdgeRanges{{40, 60}});
    FeatureWeights::Params opt_params = params;
    opt_params.optimized_in_focus = true;
    std::unique_ptr<FeatureWeights> focused_opt = FeatureWeights::Build(
        *map, opt_params, {{5, 1.0}}, FeatureWeights::EdgeRanges{{40, 60}});
    std::unique_ptr<FeatureWeights> weights_opt =
        FeatureWeights::Build(*map, opt_params, {{5, 1.0}, {50, 0.3}});
    checker->Check(
        "coverage counters: focus",
        focused->has_focus() && focused->filters_admission() &&
            focused->focus_edges() == 21 && focused->InFocus(40) &&
            focused->InFocus(60) && !focused->InFocus(61) &&
            focused->InFocus(5) && !focused->InFocus(95) &&
            focused_opt->InFocus(95) && focused->InFocus(103) &&
            !focused->InFocus(10) && !focused->InFocus(85) &&
            near(focused->Of(10), 0.1) && near(focused->Of(50), 1.0) &&
            near(focused->Of(85), 0.05) && near(focused->Of(5), 21.0) &&
            !weights->has_focus() && weights->filters_admission() &&
            weights->InFocus(10) && !weights->InFocus(95) &&
            !weights_opt->filters_admission() && weights_opt->InFocus(95),
        StringPrintf("focus edges %u, w(10) %g, w(85) %g",
                     focused->focus_edges(), focused->Of(10), focused->Of(85)));
    // Target features stay active in the rarity scheduler past rare_edge_limit.
    // Slow programs get less energy than fast ones, and partner picks favour
    // target programs in change-based fuzzing.
    FeatureWeights::SetActive(weights.get());
    auto& sel_reg = SelectorRegistry<const Program>::Instance();
    std::string sel_err;
    auto seeds =
        sel_reg.Create(SelectionConfig{"rarity", JsonValue()}, &sel_err);
    auto partners = sel_reg.Create(
        SelectionConfig{"rarity-partner", JsonValue()}, &sel_err);
    Corpus rarity_corpus(std::move(seeds), std::move(partners));
    ProgramPtr last_target, last_non_target;
    for (int i = 0; i < 10; i++) {
      Lineage l;
      auto pt = Program::Parse("let a = 1;", l, &sel_err);
      pt->edges = {5};
      if (i == 9) {
        for (uint32_t e = 15; e < 155; e++) {
          if (e != 50) {
            pt->edges.push_back(e);
          }
        }
      }
      pt->exec_time_us = i == 9 ? 1000 : 100000;
      last_target = rarity_corpus.Add(pt);
      auto pn = Program::Parse("let b = 2;", l, &sel_err);
      pn->edges = {10};
      pn->exec_time_us = 1000;
      last_non_target = rarity_corpus.Add(pn);
    }
    Random rng(42);
    bool partner_picked_target = false;
    for (int i = 0; i < 8; i++) {
      ProgramPtr p = rarity_corpus.PickPartner(*last_non_target, rng);
      if (p != nullptr && p->edges == std::vector<uint32_t>{5}) {
        partner_picked_target = true;
      }
    }
    FeatureWeights::SetActive(nullptr);
    // Without active targets, Pick() pops the max-energy seed first: the fast
    // program (1 ms) beats the slow one (100 ms) with equal edge rarity.
    auto time_seeds =
        sel_reg.Create(SelectionConfig{"rarity", JsonValue()}, &sel_err);
    auto time_partners = sel_reg.Create(
        SelectionConfig{"uniform-partner", JsonValue()}, &sel_err);
    Corpus time_corpus(std::move(time_seeds), std::move(time_partners));
    Lineage l;
    auto slow_p = Program::Parse("let x = 1;", l, &sel_err);
    slow_p->edges = {20};
    slow_p->exec_time_us = 100000;
    time_corpus.Add(slow_p);
    auto fast_p = Program::Parse("let y = 1;", l, &sel_err);
    fast_p->edges = {21};
    fast_p->exec_time_us = 1000;
    ProgramPtr fast_added = time_corpus.Add(fast_p);
    ProgramPtr first_pick = time_corpus.Pick(rng);
    const bool kept_target_5 = std::find(last_target->rare_edges.begin(),
                                         last_target->rare_edges.end(),
                                         5u) != last_target->rare_edges.end();
    checker->Check(
        "coverage counters: rarity target retention and time weighting",
        kept_target_5 && last_target->rare_edges.size() == 128 &&
            last_non_target->rare_edges.empty() && partner_picked_target &&
            first_pick == fast_added,
        StringPrintf("target rare_edges %zu, non-target %zu",
                     last_target->rare_edges.size(),
                     last_non_target->rare_edges.size()));

    std::vector<std::string> base_flags = {
        "--single-threaded",
        "--max-maglev-eager-inlined-bytecode-size=0",
        "--max-maglev-inlined-bytecode-size-small=0",
        "--always-sparkplug",
        "--verify-bytecode-light",
        "--stress-concurrent-inlining"};
    std::vector<std::string> extra_flags = {
        "--single-threaded", "--maglev-assert", "--no-sparkplug",
        "--verify-bytecode-full", "--assert-types"};
    // Keep all base flags, so that only SanitizeD8Flags removes flags.
    std::string keep_error;
    const bool set_keep = TunableRegistry::Instance().Set(
        "flags.target_keep_chance", 1.0, &keep_error);
    std::vector<std::string> combined =
        CombineD8Flags(base_flags, extra_flags, rng);
    TunableRegistry::Instance().Reset();
    const bool stripped_orphan =
        std::find(combined.begin(), combined.end(),
                  "--max-maglev-eager-inlined-bytecode-size=0") ==
            combined.end() &&
        std::find(combined.begin(), combined.end(),
                  "--max-maglev-inlined-bytecode-size-small=0") ==
            combined.end() &&
        std::find(combined.begin(), combined.end(), "--always-sparkplug") ==
            combined.end() &&
        std::find(combined.begin(), combined.end(),
                  "--verify-bytecode-light") == combined.end() &&
        std::find(combined.begin(), combined.end(),
                  "--stress-concurrent-inlining") == combined.end();
    const bool has_assert =
        std::find(combined.begin(), combined.end(), "--maglev-assert") !=
            combined.end() &&
        std::find(combined.begin(), combined.end(), "--no-sparkplug") !=
            combined.end() &&
        std::find(combined.begin(), combined.end(), "--verify-bytecode-full") !=
            combined.end();
    const ptrdiff_t st_count =
        std::count(combined.begin(), combined.end(), "--single-threaded");
    checker->Check(
        "d8 flags: CombineD8Flags deduplication and orphan stripping",
        set_keep && stripped_orphan && has_assert && st_count == 1,
        StringPrintf("size %zu, st_count %td", combined.size(), st_count));

    const bool formula_ok =
        std::abs(EffectiveTargetGuidedChance(0, 0) - 0.75) < 1e-9 &&
        std::abs(EffectiveTargetGuidedChance(3, 3) - 0.75) < 1e-9 &&
        std::abs(EffectiveTargetGuidedChance(4, 0) - 0.75) < 1e-9 &&
        std::abs(EffectiveTargetGuidedChance(10, 2) - 0.60) < 1e-9 &&
        std::abs(EffectiveTargetGuidedChance(8, 4) - 0.375) < 1e-9 &&
        std::abs(EffectiveTargetGuidedChance(10, 8) - 0.15) < 1e-9 &&
        std::abs(EffectiveTargetGuidedChance(4, 4) - 0.0) < 1e-9 &&
        std::abs(EffectiveTargetGuidedChance(20, 20) - 0.0) < 1e-9;
    checker->Check("d8 flags: EffectiveTargetGuidedChance warmup and scaling",
                   formula_ok, "");

    auto count_sentinel_draws = [](uint64_t completed, uint64_t hit,
                                   bool populate_pool) {
      FuzzConfig cfg;
      cfg.randomize_d8_flags = true;
      Fuzzer fuzzer(std::move(cfg));
      fuzzer.SetTargetsForTest({{5, 100.0}});
      if (populate_pool) {
        fuzzer.RecordTargetFlags({"--sentinel-pool-flag"}, 100.0);
      }
      for (uint64_t i = 0; i < completed; i++) {
        fuzzer.RecordTargetChild(i < hit);
      }
      Random draw_rng(12345);
      int sentinel_count = 0;
      for (int i = 0; i < 400; i++) {
        std::vector<std::string> drawn = fuzzer.DrawD8Flags(draw_rng);
        if (std::find(drawn.begin(), drawn.end(), "--sentinel-pool-flag") !=
            drawn.end()) {
          sentinel_count++;
        }
      }
      return sentinel_count;
    };
    TunableRegistry& tunable_reg = TunableRegistry::Instance();
    std::string tunable_err;
    const bool set_no_mutate =
        tunable_reg.Set("flags.target_mutate_chance", 0.0, &tunable_err);
    const int empty_pool_draws = count_sentinel_draws(10, 0, false);
    const int warmup_draws = count_sentinel_draws(3, 3, true);
    const int zero_hit_draws = count_sentinel_draws(20, 0, true);
    const int low_hit_draws = count_sentinel_draws(20, 2, true);
    const int high_hit_draws = count_sentinel_draws(20, 18, true);
    const int all_hit_draws = count_sentinel_draws(20, 20, true);
    tunable_reg.Reset();
    checker->Check(
        "d8 flags: Fuzzer scales target-guided draws by child hit rate",
        set_no_mutate && empty_pool_draws == 0 && warmup_draws > 240 &&
            zero_hit_draws > 240 && low_hit_draws > 200 && high_hit_draws > 0 &&
            high_hit_draws < 80 && low_hit_draws > high_hit_draws + 120 &&
            all_hit_draws == 0,
        StringPrintf("empty %d, warmup %d, zero %d, low %d, high %d, all %d",
                     empty_pool_draws, warmup_draws, zero_hit_draws,
                     low_hit_draws, high_hit_draws, all_hit_draws));

    // DrawD8Flags gives the d8 flags of the configuration to the new draws
    // and to the combined draws. A fixed --single-threaded thus removes
    // --stress-concurrent-inlining from all draws. A fixed
    // --maglev-non-eager-inlining keeps the inlining size 0 of the pool
    // flags in the combined draws. The Turbolev profile does not draw
    // --maglev-non-eager-inlining.
    const bool set_combined_st =
        tunable_reg.Set("flags.stress_concurrent_inlining", 1.0,
                        &tunable_err) &&
        tunable_reg.Set("flags.stress_concurrent_inlining_attach_code", 1.0,
                        &tunable_err) &&
        tunable_reg.Set("flags.target_mutate_chance", 1.0, &tunable_err) &&
        tunable_reg.Set("flags.target_keep_chance", 1.0, &tunable_err);
    FuzzConfig st_cfg;
    st_cfg.randomize_d8_flags = true;
    st_cfg.d8_flag_profile = "turbolev";
    st_cfg.target.args = {"--single-threaded", "--maglev-non-eager-inlining"};
    Fuzzer st_fuzzer(std::move(st_cfg));
    st_fuzzer.SetTargetsForTest({{5, 100.0}});
    st_fuzzer.RecordTargetFlags(
        {"--sentinel-pool-flag", "--max-maglev-eager-inlined-bytecode-size=0"},
        100.0);
    st_fuzzer.RecordTargetChild(false);
    Random st_rng(4242);
    int st_sentinel = 0;
    int st_size_zero = 0;
    bool st_had_concurrent = false;
    for (int i = 0; i < 100; i++) {
      std::vector<std::string> drawn = st_fuzzer.DrawD8Flags(st_rng);
      if (std::find(drawn.begin(), drawn.end(), "--sentinel-pool-flag") !=
          drawn.end()) {
        st_sentinel++;
        if (std::find(drawn.begin(), drawn.end(),
                      "--max-maglev-eager-inlined-bytecode-size=0") !=
            drawn.end()) {
          st_size_zero++;
        }
      }
      for (const std::string& flag : drawn) {
        if (flag.rfind("--stress-concurrent-inlining", 0) == 0) {
          st_had_concurrent = true;
        }
      }
    }
    tunable_reg.Reset();
    checker->Check(
        "d8 flags: Fuzzer draws keep the fixed d8 flags",
        set_combined_st && st_sentinel > 40 && st_sentinel < 100 &&
            st_size_zero == st_sentinel && !st_had_concurrent,
        StringPrintf("sentinel %d, size 0 %d, concurrent %d", st_sentinel,
                     st_size_zero, st_had_concurrent ? 1 : 0));

    Minimizer min_check;
    auto min_prog = Program::Parse(
        "let u0 = 1;\nlet u1 = 2;\nlet keep = 1;\n"
        "let u2 = 3;\nlet u3 = 4;\nkeep.m(1);\n",
        l, &sel_err);
    auto min_tree = min_prog->CopyTree();
    MinimizationResult min_res = min_check.Minimize(
        min_tree.get(),
        [](const std::string& src) {
          return src.find("let keep = 1") != std::string::npos &&
                 src.find("keep.m(1)") != std::string::npos;
        },
        nullptr);
    const uint64_t stmt_invocations =
        min_check.Find("statements")->stats().invocations;
    checker->Check(
        "minimizer: unreferenced declarations and fixed-point stopping",
        min_res.accepted == 1 && min_res.tests <= 8 && stmt_invocations == 1 &&
            min_res.stop == MinimizationStop::kFixedPoint,
        StringPrintf("tests %d, accepted %d, stmt_invocations %llu, stop %s",
                     min_res.tests, min_res.accepted,
                     static_cast<unsigned long long>(stmt_invocations),
                     MinimizationStopName(min_res.stop)));
  }
  munmap(mapping, CoverageMap::kShmSize);
}

// The oracle of the crash minimizer checks. The script "crashes" if its
// top-level code calls `crash`, directly or through the top-level helpers
// `var NAME = function ...`. The calls of other names do nothing.
bool ReachesCrash(const std::string& script) {
  std::unique_ptr<fast::Tree> tree =
      fast::Parser::ParseScript(script, fast::ParseOptions(), nullptr);
  if (tree == nullptr) {
    return false;
  }
  std::map<std::string, fast::Node*, std::less<>> helpers;
  std::vector<fast::Node*> work;  // the code that runs
  for (fast::Statement* statement : tree->root()->body) {
    auto* declaration = statement->As<fast::VariableDeclaration>();
    fast::VariableDeclarator* declarator =
        declaration != nullptr && declaration->declarations.size() == 1
            ? declaration->declarations[0]
            : nullptr;
    const fast::Identifier* name =
        declarator != nullptr && declarator->target != nullptr
            ? declarator->target->As<fast::Identifier>()
            : nullptr;
    if (name != nullptr && declarator->init != nullptr &&
        declarator->init->Is<fast::FunctionExpression>()) {
      helpers[std::string(name->name)] = declarator->init;
    } else {
      work.push_back(statement);
    }
  }
  std::set<std::string, std::less<>> called;
  bool crashed = false;
  while (!work.empty() && !crashed) {
    fast::Node* code = work.back();
    work.pop_back();
    fast::Walk(code, [&](fast::Node* node) {
      const auto* call = node->As<fast::CallExpression>();
      const fast::Identifier* callee =
          call != nullptr ? call->callee->As<fast::Identifier>() : nullptr;
      if (callee == nullptr) {
        return true;
      }
      if (callee->name == "crash") {
        crashed = true;
      }
      auto helper = helpers.find(callee->name);
      if (helper != helpers.end() && called.insert(helper->first).second) {
        work.push_back(helper->second);
      }
      return true;
    });
  }
  return crashed;
}

// The names of the top-level `var` declarations of `tree`, in order.
std::string TopLevelVars(const fast::Tree& tree) {
  std::string names;
  for (fast::Statement* statement : tree.root()->body) {
    const auto* declaration = statement->As<fast::VariableDeclaration>();
    if (declaration == nullptr) {
      continue;
    }
    for (const fast::VariableDeclarator* declarator :
         declaration->declarations) {
      const fast::Identifier* name =
          declarator->target != nullptr
              ? declarator->target->As<fast::Identifier>()
              : nullptr;
      if (name != nullptr) {
        names += (names.empty() ? "" : " ") + std::string(name->name);
      }
    }
  }
  return names;
}

// Offline: the crash minimizer reduces one tree of the prelude and the
// program, with the crash order of the reducers. The program calls one
// helper of the prelude, and that helper calls a second helper. The other
// helpers call each other, but the program does not reach them.
void RunCrashMinimizerChecks(Checker* checker) {
  const char* kPrelude =
      "var h0 = function(a) { return a; };\n"
      "var h1 = function(a) { return h0(a) + 1; };\n"
      "var h2 = function(a) { return crash(a); };\n"
      "var h3 = function(a) { return h2(a); };\n"
      "var h4 = function(a) { return h1(a) * 2; };\n"
      "var h5 = function(a) { return h5(a - 1); };\n"
      "var unused = 7;\n";
  std::string error;
  std::unique_ptr<ScriptTree> script =
      ScriptTree::Parse(kPrelude, "h3(1);\n", &error);
  if (!checker->Check(
          "crash minimizer: the prelude and the program parse into one tree",
          script != nullptr && script->prelude_statements() == 7 &&
              ReachesCrash(fast::Print(script->tree()->root())),
          error)) {
    return;
  }
  Minimizer minimizer;
  std::vector<Reducer*> reducers;
  for (std::string_view name : Minimizer::CrashOrder()) {
    reducers.push_back(minimizer.Find(name));
  }
  const MinimizationResult result =
      minimizer.MinimizeWith(reducers, script->tree(), ReachesCrash, nullptr,
                             FuzzConfig().crash_minimizer);
  const std::string kept = TopLevelVars(*script->tree());
  std::string program = script->PrintProgram();
  std::replace(program.begin(), program.end(), '\n', ' ');
  checker->Check(
      "crash minimizer: the program keeps exactly the prelude helpers that it "
      "reaches",
      kept == "h2 h3" && script->PreludeStatementsNow() == 2 &&
          program.find("h3(") != std::string::npos &&
          program.find("var ") == std::string::npos &&
          result.stop == MinimizationStop::kFixedPoint,
      StringPrintf("helpers \"%s\", %zu prelude statements, program \"%s\", "
                   "stop %s, %d tests",
                   kept.c_str(), script->PreludeStatementsNow(),
                   program.c_str(), MinimizationStopName(result.stop),
                   result.tests));
  // `var a = h0` and `(1);` parse as one statement: `var a = h0(1);`.
  std::unique_ptr<ScriptTree> joined =
      ScriptTree::Parse("var a = h0", "(1);\n", &error);
  checker->Check(
      "crash minimizer: no tree for a prelude that continues in the program",
      joined == nullptr, joined == nullptr ? error : "one tree");
}

// Offline: the search of the d8 flags of NAME.repro.js (MinimizeFlags). The
// oracle needs 2 flags of 7, and --allow-natives-syntax must stay.
void RunFlagMinimizerChecks(Checker* checker) {
  const std::vector<std::string> flags = {
      "--a", "--b", "--allow_natives_syntax", "--c=1", "--d", "--e", "--f"};
  int calls = 0;
  auto oracle = [&calls](const std::vector<std::string>& candidate) {
    calls++;
    auto has = [&candidate](const char* flag) {
      return std::find(candidate.begin(), candidate.end(), flag) !=
             candidate.end();
    };
    return has("--c=1") && has("--f");
  };
  auto keep = [](const std::string& flag) {
    return IsFlag(flag, "allow-natives-syntax");
  };
  const FlagMinimization result =
      MinimizeFlags(flags, keep, oracle, [] { return false; });
  std::string kept;
  for (const std::string& flag : result.flags) {
    kept += (kept.empty() ? "" : " ") + flag;
  }
  checker->Check(
      "crash flags: the search keeps the needed flags and the kept flags",
      kept == "--allow_natives_syntax --c=1 --f" && result.tests == calls &&
          !result.budget_exhausted,
      StringPrintf("kept \"%s\", %d tests", kept.c_str(), result.tests));
  int tests = 0;
  const FlagMinimization stopped =
      MinimizeFlags(flags, keep, oracle, [&tests] { return tests++ >= 2; });
  checker->Check("crash flags: the search stops at the end of the budget",
                 stopped.budget_exhausted && stopped.tests == 2,
                 StringPrintf("%d tests, budget_exhausted %d", stopped.tests,
                              stopped.budget_exhausted ? 1 : 0));
  // HasNativeCall is also true for a script that does not parse. Thus the
  // script with the call must parse.
  const char* call_in_function =
      "function f() { return %DebugPrint(1); }\nf();";
  const bool in_function =
      fast::Parser::ParseScript(call_in_function, fast::ParseOptions(),
                                nullptr) != nullptr &&
      HasNativeCall(call_in_function);
  const bool plain = HasNativeCall("print(1);");
  const bool not_parsing = HasNativeCall("let = ;");
  checker->Check(
      "crash flags: % calls need --allow-natives-syntax",
      in_function && !plain && not_parsing &&
          IsFlag("--allow_natives_syntax=1", "allow-natives-syntax") &&
          !IsFlag("--allow-natives-syntax-x", "allow-natives-syntax"),
      StringPrintf("call in a function %d, no call %d, no parse %d",
                   in_function ? 1 : 0, plain ? 1 : 0, not_parsing ? 1 : 0));
  checker->Check(
      "crash flags: the search keeps the flags for a deterministic run",
      KeepReproFlag("--single-threaded", false) &&
          KeepReproFlag("--random-seed=7", false) &&
          KeepReproFlag("--allow-natives-syntax", true) &&
          !KeepReproFlag("--allow-natives-syntax", false) &&
          !KeepReproFlag("--jit-fuzzing", true),
      "");
}

// Offline: the plain test of an amplified crash. Its processes follow the
// crash rate of the amplify step. Its flags have the flags of the amplify
// step instead of --stress-runs and --random-seed. Also the decisions of the
// rate checks of NAME.repro.js, and the cause of a stop in their notes.
void RunAmplifiedTestChecks(Checker* checker) {
  const std::string runs = StringPrintf(
      "%d %d %d %d %d %d", AmplifiedRuns(5, 5), AmplifiedRuns(4, 5),
      AmplifiedRuns(1, 2), AmplifiedRuns(1, 5), AmplifiedRuns(0, 5),
      AmplifiedRuns(3, 0));
  checker->Check("amplified crashes: the processes of a test follow the rate",
                 runs == "2 3 5 9 10 10", runs);
  std::string flags;
  for (const std::string& flag :
       AmplifiedFlags({"--a", "--random-seed=5", "--stress_runs=3", "--b"},
                      "--c --stress-runs=24")) {
    flags += (flags.empty() ? "" : " ") + flag;
  }
  // The rate check: a bar of 4 of 5.
  auto decide = [](RateCount kept, const RateCount* all) {
    const RateDecision d = DecideAmplifiedRate(4, 5, kept, all);
    return StringPrintf("%s%s %d/%d", d.keep ? "keep" : "drop",
                        d.all_flags ? " all" : "", d.rate.hits, d.rate.runs);
  };
  const RateCount all_pass{5, 5}, all_fail{5, 3}, all_cut{2, 2};
  const std::string decisions =
      decide({5, 4}, nullptr) + ", " + decide({5, 3}, &all_pass) + ", " +
      decide({5, 3}, &all_fail) + ", " + decide({3, 3}, nullptr) + ", " +
      decide({5, 3}, &all_cut) + ", " + decide({5, 2}, nullptr);
  checker->Check(
      "amplified crashes: the rate check keeps only a complete count that "
      "passes",
      decisions ==
          "keep 4/5, keep all 5/5, drop 3/5, drop 3/3, drop 2/2, "
          "drop 2/5",
      decisions);
  // The rate check of a crash that reproduces alone: the bar is 5 of 5, and
  // NAME.repro.js always stays.
  auto decide_alone = [](RateCount kept, const RateCount* all) {
    const RateDecision d =
        DecideReproducibleRate(5, kept, all, "the check stopped");
    return StringPrintf("%s%s %d/%d", d.keep ? "keep" : "drop",
                        d.all_flags ? " all" : "", d.rate.hits, d.rate.runs);
  };
  const std::string alone =
      decide_alone({5, 5}, nullptr) + ", " + decide_alone({5, 4}, &all_pass) +
      ", " + decide_alone({5, 4}, &all_fail) + ", " +
      decide_alone({5, 4}, nullptr) + ", " + decide_alone({2, 2}, nullptr) +
      ", " + decide_alone({5, 4}, &all_cut);
  // The note gives the cause of a count that stopped early.
  const std::string cut_note =
      DecideAmplifiedRate(4, 5, {0, 0}, nullptr, "cannot start d8: x").note;
  const std::string alone_note =
      DecideReproducibleRate(5, {5, 4}, &all_cut, "the session stopped").note;
  checker->Check(
      "crash flags: the rate check of a crash that reproduces alone keeps "
      "the flags of the search only after 5 of 5",
      alone == "keep 5/5, keep all 5/5, keep all 3/5, keep all 4/5, keep all "
               "2/2, keep all 2/2" &&
          cut_note ==
              "the bar is 4 of 5, not checked after 0 of 5 runs: cannot "
              "start d8: x, so Redline wrote no NAME.repro.js" &&
          alone_note ==
              "the bar is 5 of 5, the kept flags gave 4 of 5, the check of "
              "all flags stopped after 2 of 5 runs: the session stopped",
      alone + "; " + cut_note + "; " + alone_note);
  checker->Check("amplified crashes: the flags of a test",
                 flags == "--a --b --c --stress-runs=24" &&
                     KeepReproFlag("--stress-runs=24", false),
                 flags);
}

// Offline: the steps of the bundle minimization (MinimizeBundleParts). In
// the first case, the test needs the part "needed" and the crash part last.
// The time limit ends after the first step, so no part gets a reduction. The
// second case has no time limit, and the reductions change the needed parts.
void RunBundleStepChecks(Checker* checker) {
  int tests = 0;
  int reductions = 0;
  const std::vector<std::string> kept = MinimizeBundleParts(
      {"unneeded", "needed", "other", "crash"},
      [&tests](const std::vector<std::string>& candidate) {
        tests++;
        return std::find(candidate.begin(), candidate.end(), "needed") !=
                   candidate.end() &&
               candidate.back() == "crash";
      },
      [&reductions](std::vector<std::string>*, size_t) { reductions++; },
      [&tests] { return tests >= 4; });
  std::string text;
  for (const std::string& part : kept) {
    text += (text.empty() ? "" : " ") + part;
  }
  checker->Check(
      "bundles: the steps keep the needed part and the crash part last",
      text == "needed crash" && reductions == 0,
      StringPrintf("parts \"%s\", %d tests, %d reductions", text.c_str(), tests,
                   reductions));
  // Without a time limit. The reduction of the crash part makes the part
  // "a" unnecessary, and the reduction of "b" gives "b2". The removal after
  // each step must remove the part that is not necessary.
  int reduced = 0;
  const std::vector<std::string> again = MinimizeBundleParts(
      {"a", "b", "crash"},
      [](const std::vector<std::string>& candidate) {
        auto has = [&candidate](const char* part) {
          return std::find(candidate.begin(), candidate.end(), part) !=
                 candidate.end();
        };
        if (candidate.back() == "crash") {
          return has("a") && has("b");
        }
        return candidate.back() == "crash2" && (has("b") || has("b2"));
      },
      [&reduced](std::vector<std::string>* parts, size_t i) {
        reduced++;
        std::string& part = (*parts)[i];
        part = part == "crash" ? "crash2" : part == "b" ? "b2" : part;
      },
      [] { return false; });
  std::string again_text;
  for (const std::string& part : again) {
    again_text += (again_text.empty() ? "" : " ") + part;
  }
  checker->Check(
      "bundles: a reduced crash part lets the steps remove a history part",
      again_text == "b2 crash2" && reduced == 2,
      StringPrintf("parts \"%s\", %d reductions", again_text.c_str(), reduced));
}

// The value of the line `key: value` of a crash report, or "".
std::string ReportValue(const std::string& report, const std::string& key) {
  const std::string prefix = key + ": ";
  size_t start = 0;
  while (start < report.size()) {
    size_t end = report.find('\n', start);
    if (end == std::string::npos) {
      end = report.size();
    }
    if (report.compare(start, prefix.size(), prefix) == 0) {
      return report.substr(start + prefix.size(), end - start - prefix.size());
    }
    start = end + 1;
  }
  return "";
}

// The time that the session of a crash check can take. The crash minimizer
// stops after crash_minimizer.max_time_ms.
constexpr double kCrashSessionSeconds = 300;

// The time that a plain d8 run of a NAME.repro.js file can take.
constexpr uint64_t kPlainRunUs = 60'000'000;

// Removes `path`. For a directory, also removes all of its contents.
void RemoveTree(const std::string& path) {
  struct stat st;
  if (lstat(path.c_str(), &st) != 0) {
    return;
  }
  if (!S_ISDIR(st.st_mode)) {
    unlink(path.c_str());
    return;
  }
  std::vector<std::string> names;
  if (DIR* dir = opendir(path.c_str())) {
    while (const struct dirent* entry = readdir(dir)) {
      const std::string entry_name = entry->d_name;
      if (entry_name != "." && entry_name != "..") {
        names.push_back(entry_name);
      }
    }
    closedir(dir);
  }
  for (const std::string& entry_name : names) {
    RemoveTree(path + "/" + entry_name);
  }
  rmdir(path.c_str());
}

// The result of a short session that imports crash programs.
struct CrashSession {
  std::string dir;     // the files of the session, or "" if there are none
  bool saved = false;  // the session saved a crash
  // Why the wait stopped: "crash saved", "session ended", "imports did not
  // crash" or "time limit".
  std::string stop;
  double seconds = 0;
  std::string report;  // the report of the crash with NAME.repro.js, or ""
  std::string repro;   // its NAME.repro.js, or ""
  std::string error;   // why the session did not run, or ""
};

// Runs a short session with the d8 flags of `target` and `session_flags`. The
// session imports `programs`, in an order that the seed of the session sets.
// The wait stops when the session saved a crash, when Run() returns, when
// the imports did not crash or after kCrashSessionSeconds. The caller
// removes `dir` with RemoveTree().
CrashSession RunCrashSession(const TargetOptions& target,
                             const std::vector<std::string>& session_flags,
                             const std::vector<std::string>& programs,
                             const std::string& report_key = "repro") {
  CrashSession session;
  char dir_template[] = "/tmp/redline-selftest.XXXXXX";
  if (mkdtemp(dir_template) == nullptr) {
    session.error = StringPrintf("mkdtemp: %s", strerror(errno));
    return session;
  }
  session.dir = dir_template;
  const std::string import_dir = session.dir + "/import";
  if (!MakeDirectories(import_dir, &session.error)) {
    return session;
  }
  for (size_t i = 0; i < programs.size(); i++) {
    if (!WriteStringToFile(StringPrintf("%s/%02zu.js", import_dir.c_str(), i),
                           programs[i], &session.error)) {
      return session;
    }
  }
  std::string args;
  for (const std::string& arg : target.args) {
    args += (args.empty() ? "" : " ") + arg;
  }
  std::vector<std::string> all_flags = {
      "redline",         "fuzz",    "--jobs=1",         "--seed=1",
      "--no-introspect", "--quiet", "--no-save-corpus", "--no-component-stats"};
  all_flags.push_back("--d8=" + target.executable);
  all_flags.push_back(std::string("--target-kind=") +
                      TargetKindName(target.kind));
  all_flags.push_back("--d8-args=" + args);
  // The d8 environment of the selftest, for example LD_PRELOAD.
  std::string env;
  for (const auto& [key, value] : target.extra_env) {
    env += (env.empty() ? "" : ",") + key + "=" + value;
  }
  if (!env.empty()) {
    all_flags.push_back("--d8-env=" + env);
  }
  all_flags.push_back("--import=" + import_dir);
  all_flags.push_back("--out=" + session.dir + "/out");
  all_flags.insert(all_flags.end(), session_flags.begin(), session_flags.end());
  std::vector<const char*> argv;
  for (const std::string& flag : all_flags) {
    argv.push_back(flag.c_str());
  }
  Flags flags = Flags::Parse(static_cast<int>(argv.size()), argv.data(), 2);
  FuzzConfig config;
  if (!FuzzConfig::Load(flags, &config, &session.error)) {
    return session;
  }
  // The session logs to stderr. Show only its errors.
  const LogLevel log_level = GetLogLevel();
  SetLogLevel(LogLevel::kError);
  const std::string crashes = session.dir + "/out/crashes";
  Stopwatch clock;
  {
    Fuzzer fuzzer(std::move(config));
    if (!fuzzer.Initialize(&session.error)) {
      SetLogLevel(log_level);
      return session;
    }
    std::atomic<bool> ended{false};
    std::thread thread([&fuzzer, &ended] {
      fuzzer.Run();
      ended.store(true);
    });
    // The session writes signatures.txt after the files of a crash. The
    // worker processes a crash of an import before it starts to fuzz. Thus
    // a fuzz execution without signatures.txt shows that the imports did not
    // crash.
    while (session.stop.empty()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(200));
      // Read the two flags before the file, so that a crash that the
      // session saves in the meantime counts as saved.
      const bool session_ended = ended.load();
      const bool fuzzing = fuzzer.stats().fuzz_executions.load() > 0;
      session.saved = FileExists(crashes + "/signatures.txt");
      if (session.saved) {
        session.stop = "crash saved";
      } else if (session_ended) {
        session.stop = "session ended";
      } else if (fuzzing) {
        session.stop = "imports did not crash";
      } else if (clock.ElapsedSeconds() >= kCrashSessionSeconds) {
        session.stop = "time limit";
      }
    }
    fuzzer.RequestStop();
    thread.join();
  }
  SetLogLevel(log_level);
  session.seconds = clock.ElapsedSeconds();
  // The report of an imported program: the report with the line
  // `report_key`, or else the first report. A crash of the fuzzing after the
  // imports gets no NAME.repro.js, because the stop request interrupts it.
  std::string error, first;
  for (const std::string& path : ListFiles(crashes, ".txt")) {
    std::string text;
    if (!BaseName(path).starts_with("crash_") ||
        !ReadFileToString(path, &text, &error)) {
      continue;
    }
    if (first.empty()) {
      first = text;
    }
    if (!ReportValue(text, report_key).empty()) {
      session.report = text;
    }
  }
  if (session.report.empty()) {
    session.report = first;
  }
  const std::string repro_name = ReportValue(session.report, "repro");
  // The report and NAME.repro.js are also the result of a session with
  // another `report_key`.
  if (!repro_name.empty() &&
      !ReadFileToString(crashes + "/" + repro_name, &session.repro, &error)) {
    session.error = error;
  }
  return session;
}

// The words of the `// run:` line of a NAME.repro.js file: the d8, its
// flags and the file. Empty if there is no run line.
std::vector<std::string> RunLineWords(const std::string& repro) {
  const std::string prefix = "\n// run: ";
  const size_t start = repro.find(prefix);
  if (start == std::string::npos) {
    return {};
  }
  const size_t end = repro.find('\n', start + 1);
  const std::string line =
      repro.substr(start + prefix.size(), end - start - prefix.size());
  std::vector<std::string> words;
  size_t position = 0;
  while (position < line.size()) {
    const size_t space = std::min(line.find(' ', position), line.size());
    if (space > position) {
      words.push_back(line.substr(position, space - position));
    }
    position = space + 1;
  }
  return words;
}

// Runs the `// run:` line of a NAME.repro.js file in a plain d8 process.
// Returns the signature of the crash. If there is no crash, returns the
// reason in parentheses.
std::string RunReproLine(const std::string& repro,
                         const TargetOptions& target) {
  const std::vector<std::string> words = RunLineWords(repro);
  if (words.empty()) {
    return "(no run line)";
  }
  const std::vector<std::string> args(words.begin() + 1, words.end());
  const ProcessResult run =
      RunProcess(words[0], args, target.PlainEnvironment(), kPlainRunUs);
  if (!run.started) {
    return "(cannot start d8: " + run.error + ")";
  }
  if (run.timed_out) {
    return "(timed out)";
  }
  if (run.signal == 0 && run.exit_code == 0) {
    return "(no crash)";
  }
  return CrashSignature::FromOutput(run.signal, run.exit_code, run.stderr_text)
      .text;
}

// A crash behind the default prelude, in a short session. The crash
// minimizer reduces the tree of the prelude and the program. NAME.repro.js
// must keep the 2 prelude helpers that the program reaches, and no other
// prelude statement. Its `// run:` line must have absolute paths, and it
// must crash a plain d8 process with the signature of the crash. The node
// counts of `minimized:` are the counts of the program alone.
void RunCrashReproCheck(Checker* checker, const TargetOptions& target,
                        bool fuzzilli_target) {
  const char* name =
      "crash minimizer: a crash behind the default prelude gives "
      "NAME.repro.js without the unused helpers";
  // redline('REDLINE_CRASH', 1) or fuzzilli('FUZZILLI_CRASH', 1) is a CHECK
  // failure (see the crash checks).
  const char* crash_call =
      fuzzilli_target ? "fuzzilli('FUZZILLI_CRASH'" : "redline('REDLINE_CRASH'";
  const CrashSession session = RunCrashSession(
      target, {}, {StringPrintf("assertThrows(() => %s, 1));\n", crash_call)});
  const std::string& repro = session.repro;
  // Two header lines, then the script.
  const size_t first_line = repro.find('\n');
  const size_t second_line = first_line == std::string::npos
                                 ? first_line
                                 : repro.find('\n', first_line + 1);
  const bool header = repro.starts_with("// signature: ") &&
                      second_line != std::string::npos &&
                      repro.compare(first_line + 1, 8, "// run: ") == 0;
  std::unique_ptr<fast::Tree> tree =
      header ? fast::Parser::ParseScript(repro.substr(second_line + 1),
                                         fast::ParseOptions(), nullptr)
             : nullptr;
  const std::string helpers = tree != nullptr ? TopLevelVars(*tree) : "";
  const std::string& report = session.report;
  const std::string signature = ReportValue(report, "signature");
  const std::string plain = header ? RunReproLine(repro, target) : "";
  const std::vector<std::string> words = RunLineWords(repro);
  const bool absolute = words.size() >= 2 && words[0].starts_with("/") &&
                        words.back().starts_with("/");
  // The nodes of the program in the file NAME + `suffix` of the crash, or 0.
  // NAME.orig.js has the program alone before the minimization, and NAME.js
  // has it after the minimization.
  const std::string repro_name = ReportValue(report, "repro");
  auto file_nodes = [&](const char* suffix) -> size_t {
    const std::string repro_suffix = ".repro.js";
    std::string text, error;
    if (!repro_name.ends_with(repro_suffix) ||
        !ReadFileToString(
            session.dir + "/out/crashes/" +
                repro_name.substr(0, repro_name.size() - repro_suffix.size()) +
                suffix,
            &text, &error)) {
      return 0;
    }
    std::unique_ptr<fast::Tree> file_tree =
        fast::Parser::ParseScript(text, fast::ParseOptions(), nullptr);
    return file_tree != nullptr ? fast::CountNodes(file_tree->root()) : 0;
  };
  const size_t orig_nodes = file_nodes(".orig.js");
  const size_t js_nodes = file_nodes(".js");
  RemoveTree(session.dir);
  const std::string min_tree = ReportValue(report, "min_tree");
  const std::string prelude = ReportValue(report, "min_prelude_statements");
  const std::string stop = ReportValue(report, "min_stop");
  const std::string minimized = ReportValue(report, "minimized");
  const std::string script_nodes = ReportValue(report, "min_script_nodes");
  size_t nodes_before = 0, nodes_after = 0, script_before = 0, script_after = 0;
  const bool counts =
      sscanf(minimized.c_str(), "yes (%*d tests, %zu -> %zu nodes)",
             &nodes_before, &nodes_after) == 2 &&
      sscanf(script_nodes.c_str(), "%zu -> %zu", &script_before,
             &script_after) == 2;
  const bool ok = tree != nullptr && helpers == "__redline_call assertThrows" &&
                  min_tree == "prelude and program" &&
                  prelude.ends_with("-> 2") && !stop.empty() &&
                  stop != "skipped" && counts && orig_nodes > 0 &&
                  nodes_before == orig_nodes && nodes_after == js_nodes &&
                  script_before > nodes_before && absolute &&
                  ReportValue(report, "repro_rate") == "5/5" &&
                  !signature.empty() && plain == signature;
  checker->Check(
      name, ok,
      StringPrintf("%s, %.0f s, helpers \"%s\", min_tree %s, "
                   "min_prelude_statements %s, min_stop %s, minimized %s, "
                   "min_script_nodes %s, NAME.orig.js %zu nodes, NAME.js %zu "
                   "nodes, run line "
                   "\"%s ... %s\", repro_rate %s, plain d8 run \"%s\", "
                   "signature \"%s\"%s%s",
                   session.stop.c_str(), session.seconds, helpers.c_str(),
                   min_tree.c_str(), prelude.c_str(), stop.c_str(),
                   minimized.c_str(), script_nodes.c_str(), orig_nodes,
                   js_nodes, words.empty() ? "" : words[0].c_str(),
                   words.empty() ? "" : words.back().c_str(),
                   ReportValue(report, "repro_rate").c_str(), plain.c_str(),
                   signature.c_str(), session.error.empty() ? "" : ", ",
                   session.error.c_str()));
}

// A crash behind a prelude whose text matters: the program reads the text
// of a prelude function. The printer removes the comment in the function,
// so the printed tree of the prelude and the program does not crash. Then the
// minimizer reduces the program alone, behind the exact prelude. Redline writes
// no NAME.repro.js, and `min_tree` gives the reason.
void RunCrashFallbackCheck(Checker* checker, const TargetOptions& target,
                           bool fuzzilli_target) {
  const char* name =
      "crash minimizer: a prelude whose text matters gives the program alone "
      "and no NAME.repro.js";
  char dir_template[] = "/tmp/redline-selftest-prelude.XXXXXX";
  std::string error, prelude_dir;
  if (mkdtemp(dir_template) == nullptr) {
    error = StringPrintf("mkdtemp: %s", strerror(errno));
  } else {
    prelude_dir = dir_template;
  }
  const std::string prelude = prelude_dir + "/prelude.js";
  if (error.empty()) {
    WriteStringToFile(prelude,
                      "var f = function() { /*keep*/ return 1; };\n"
                      "var unused = 2;\n",
                      &error);
  }
  CrashSession session;
  if (error.empty()) {
    const char* crash_call = fuzzilli_target ? "fuzzilli('FUZZILLI_CRASH'"
                                             : "redline('REDLINE_CRASH'";
    session = RunCrashSession(
        target, {"--prelude=" + prelude},
        {StringPrintf("if (f.toString().includes(\"/*keep*/\")) %s, 1);\n",
                      crash_call)});
    error = session.error;
    RemoveTree(session.dir);
  }
  if (!prelude_dir.empty()) {
    RemoveTree(prelude_dir);
  }
  const std::string& report = session.report;
  const std::string min_tree = ReportValue(report, "min_tree");
  const bool ok = error.empty() && !ReportValue(report, "signature").empty() &&
                  min_tree.starts_with("program alone: the printed script") &&
                  ReportValue(report, "repro").empty() && session.repro.empty();
  checker->Check(
      name, ok,
      StringPrintf("%s, %.0f s, min_tree \"%s\", repro \"%s\"%s%s",
                   session.stop.c_str(), session.seconds, min_tree.c_str(),
                   ReportValue(report, "repro").c_str(),
                   error.empty() ? "" : ", ", error.c_str()));
}

// A crash that needs 1 flag of the session, in a short session with the
// default prelude. The search of the d8 flags of NAME.repro.js keeps only
// this flag and the flags that KeepReproFlag keeps. The time budget is
// large, so only the process limit stops the search. The `// run:` line has
// these flags, and it crashes a plain d8 process with the signature. The
// rate check keeps these flags, and no temporary file stays.
void RunCrashFlagsCheck(Checker* checker, const TargetOptions& target,
                        bool fuzzilli_target) {
  const char* name =
      "crash flags: a crash that needs 1 flag of the session keeps only this "
      "flag";
  // Crash kind 7 crashes only with --hole-fuzzing.
  const char* crash_call =
      fuzzilli_target ? "fuzzilli('FUZZILLI_CRASH'" : "redline('REDLINE_CRASH'";
  const CrashSession session =
      RunCrashSession(target,
                      {"--d8-extra-args=--hole-fuzzing",
                       R"(--config={"crash_flag_budget_sec": 1000})"},
                      {StringPrintf("%s, 7);\n", crash_call)});
  // The flags that the search keeps, in the order of the target.
  std::vector<std::string> expected;
  for (const std::string& arg : target.args) {
    if (KeepReproFlag(arg, /*natives=*/false)) {
      expected.push_back(arg);
    }
  }
  expected.push_back("--hole-fuzzing");
  std::string expected_text;
  for (const std::string& flag : expected) {
    expected_text += (expected_text.empty() ? "" : " ") + flag;
  }
  const std::string flags = ReportValue(session.report, "repro_flags");
  const std::vector<std::string> words = RunLineWords(session.repro);
  const std::string plain =
      words.empty() ? "" : RunReproLine(session.repro, target);
  const size_t temporary =
      ListFiles(session.dir + "/out/crashes", ".tmp").size();
  RemoveTree(session.dir);
  const std::string signature = ReportValue(session.report, "signature");
  const std::string note = ReportValue(session.report, "repro_flags_note");
  const std::string rate = ReportValue(session.report, "repro_rate");
  const bool ok =
      flags == expected_text && words.size() == expected.size() + 2 &&
      std::equal(expected.begin(), expected.end(), words.begin() + 1) &&
      note.find(", budget used up") == std::string::npos &&
      note.find(", the session stopped") == std::string::npos &&
      rate == "5/5" && temporary == 0 && !signature.empty() &&
      plain == signature;
  checker->Check(
      name, ok,
      StringPrintf("%s, %.0f s, repro_flags \"%s\" (expected \"%s\"), %s, "
                   "repro_rate %s (%s), %zu temporary files, %zu words in the "
                   "run line, plain d8 run \"%s\", signature \"%s\"%s%s",
                   session.stop.c_str(), session.seconds, flags.c_str(),
                   expected_text.c_str(), note.c_str(), rate.c_str(),
                   ReportValue(session.report, "repro_rate_note").c_str(),
                   temporary, words.size(), plain.c_str(), signature.c_str(),
                   session.error.empty() ? "" : ", ", session.error.c_str()));
}

// A program that changes one more protector of the isolate in each run. The
// protectors stay changed in the next runs of the same process, so the 5th
// run in one process crashes. `type` names its class, so that two programs
// can have different sources. The last statement is an unused declaration.
// The dataflow reducer tests the later statements first, so the first test
// of the minimizer removes this declaration, and the test passes.
std::string ProtectorProgram(const char* type, bool fuzzilli_target) {
  const char* crash_call =
      fuzzilli_target ? "fuzzilli('FUZZILLI_CRASH'" : "redline('REDLINE_CRASH'";
  return StringPrintf(
      "class %s {}\n"
      "if (!%%TypedArraySpeciesProtector()) {\n"
      "  %s, 1);\n"
      "} else if (!%%RegExpSpeciesProtector()) {\n"
      "  Object.defineProperty(Uint8Array, Symbol.species, {value: %s});\n"
      "} else if (!%%PromiseSpeciesProtector()) {\n"
      "  Object.defineProperty(RegExp, Symbol.species, {value: %s});\n"
      "} else if (!%%ArraySpeciesProtector()) {\n"
      "  Object.defineProperty(Promise, Symbol.species, {value: %s});\n"
      "} else {\n"
      "  Object.defineProperty(Array, Symbol.species, {value: %s});\n"
      "}\n"
      "var unused = 0;\n",
      type, crash_call, type, type, type, type);
}

// An amplified crash, in a short session without the prelude, with
// harvest_crashes. Each run of the program changes one more protector of the
// isolate. The protectors stay changed in the next runs of the same process,
// and the 5th run crashes. Thus a fresh target with crash_reruns runs does
// not crash, and the amplify step finds --stress-runs. The minimizer reduces
// the program with the amplified flags in plain d8 processes. The `// run:`
// line of NAME.repro.js keeps --stress-runs, and it crashes a plain d8
// process with the signature.
void RunCrashAmplifiedCheck(Checker* checker, const TargetOptions& target,
                            bool fuzzilli_target) {
  const char* name =
      "amplified crashes: an amplified crash gets NAME.repro.js with "
      "--stress-runs";
  // With min_reruns 5, the 4th determinism rerun of the first program is the
  // 5th run, and HarvestCrash takes its crash. The second program, with a
  // different source, makes the test independent of the number of reruns.
  // The time limit of 2 s stops the minimization before its end. The time
  // limit starts after the start check, and the first test of the minimizer
  // removes the unused declaration (ProtectorProgram). Thus the minimizer
  // accepts this test also on a loaded machine, and the last check must run
  // after the time limit.
  const CrashSession session = RunCrashSession(
      target,
      {"--prelude=none",
       R"(--config={"crash_history": 0, "crash_amplified_budget_sec": 2, "harvest_crashes": true})"},
      {ProtectorProgram("C0", fuzzilli_target),
       ProtectorProgram("C1", fuzzilli_target)});
  const std::string& report = session.report;
  const std::vector<std::string> words = RunLineWords(session.repro);
  const std::string plain =
      words.empty() ? "" : RunReproLine(session.repro, target);
  RemoveTree(session.dir);
  const std::string signature = ReportValue(report, "signature");
  const bool stress_runs = std::any_of(
      words.begin(), words.end(),
      [](const std::string& word) { return IsFlag(word, "stress-runs"); });
  const std::string stop = ReportValue(report, "min_stop");
  const bool ok = ReportValue(report, "amplified") == "yes" &&
                  ReportValue(report, "reproducible") == "no" &&
                  ReportValue(report, "min_oracle").starts_with("1 of ") &&
                  stop == "budget" &&
                  !ReportValue(report, "repro_rate").empty() && stress_runs &&
                  !signature.empty() && plain == signature;
  checker->Check(
      name, ok,
      StringPrintf(
          "%s, %.0f s, amplified_rate %s, min_oracle \"%s\", min_stop %s, "
          "min_tree \"%s\", min_script_nodes %s, repro_flags \"%s\", %s, "
          "repro_rate %s (%s), plain d8 run \"%s\", signature \"%s\"%s%s",
          session.stop.c_str(), session.seconds,
          ReportValue(report, "amplified_rate").c_str(),
          ReportValue(report, "min_oracle").c_str(), stop.c_str(),
          ReportValue(report, "min_tree").c_str(),
          ReportValue(report, "min_script_nodes").c_str(),
          ReportValue(report, "repro_flags").c_str(),
          ReportValue(report, "repro_flags_note").c_str(),
          ReportValue(report, "repro_rate").c_str(),
          ReportValue(report, "repro_rate_note").c_str(), plain.c_str(),
          signature.c_str(), session.error.empty() ? "" : ", ",
          session.error.c_str()));
}

// A state-dependent crash, in a short session without the prelude and
// without the amplify step. The programs are the programs of
// RunCrashAmplifiedCheck. The history replay finds the earlier runs, and
// Redline writes NAME.bundle.js. The minimizer writes NAME.bundle.min.js
// with fewer parts. It crashes as often as NAME.bundle.js, and it crashes a
// plain d8 process with the signature.
void RunCrashBundleCheck(Checker* checker, const TargetOptions& target,
                         bool fuzzilli_target) {
  const char* name =
      "bundles: a state-dependent crash gets a smaller NAME.bundle.min.js";
  const ProcessResult probe =
      RunProcess(target.executable, {"--bundle-reprl", "/dev/null"},
                 target.PlainEnvironment(), kPlainRunUs);
  if (!probe.started || probe.timed_out || probe.signal != 0 ||
      probe.exit_code != 0 ||
      probe.stderr_text.find("unknown flag --bundle-reprl") !=
          std::string::npos) {
    checker->Info(name, "skipped (the target d8 has no --bundle-reprl flag)");
    return;
  }
  const CrashSession session = RunCrashSession(
      target,
      {"--prelude=none",
       R"(--config={"amplify": {"strategy": "none"}, "crash_bundle_min_budget_sec": 60})"},
      {ProtectorProgram("C0", fuzzilli_target),
       ProtectorProgram("C1", fuzzilli_target)},
      "bundle_min");
  const std::string& report = session.report;
  const std::string crashes = session.dir + "/out/crashes/";
  const std::string bundle_name = ReportValue(report, "bundle");
  const std::string min_name = ReportValue(report, "bundle_min");
  std::string bundle, min, error;
  const bool read = !bundle_name.empty() && !min_name.empty() &&
                    ReadFileToString(crashes + bundle_name, &bundle, &error) &&
                    ReadFileToString(crashes + min_name, &min, &error);
  // One plain d8 run of the minimized bundle.
  std::string plain;
  if (read) {
    std::vector<std::string> args = target.args;
    args.push_back("--bundle-reprl");
    args.push_back(crashes + min_name);
    const ProcessResult run = RunProcess(
        target.executable, args, target.PlainEnvironment(), kPlainRunUs);
    plain = !run.started || run.timed_out ? "(no run)"
            : run.signal == 0 && run.exit_code == 0
                ? "(no crash)"
                : CrashSignature::FromOutput(run.signal, run.exit_code,
                                             run.stderr_text)
                      .text;
  }
  RemoveTree(session.dir);
  auto parts = [](const std::string& text) {
    size_t count = 0;
    for (size_t at = text.find(kBundleScriptMarker); at != std::string::npos;
         at = text.find(kBundleScriptMarker, at + 1)) {
      count++;
    }
    return count;
  };
  // The hits of a rate "h/n".
  auto hits = [](const std::string& rate) {
    return rate.empty() ? -1 : atoi(rate.c_str());
  };
  const std::string signature = ReportValue(report, "signature");
  const std::string rate = ReportValue(report, "bundle_rate");
  const std::string min_rate = ReportValue(report, "bundle_min_rate");
  const bool ok = ReportValue(report, "state_dependent") == "yes" && read &&
                  parts(min) < parts(bundle) && hits(min_rate) >= hits(rate) &&
                  hits(rate) > 0 && !signature.empty() && plain == signature;
  checker->Check(
      name, ok,
      StringPrintf("%s, %.0f s, bundle_rate %s, bundle_min_rate %s, "
                   "bundle_min_note \"%s\", %zu -> %zu parts, plain d8 run "
                   "\"%s\", signature \"%s\"%s%s",
                   session.stop.c_str(), session.seconds, rate.c_str(),
                   min_rate.c_str(),
                   ReportValue(report, "bundle_min_note").c_str(),
                   parts(bundle), parts(min), plain.c_str(), signature.c_str(),
                   error.empty() ? "" : ", ", error.c_str()));
}

// Offline: optimized code features with one bit per edge. As with counters,
// they do not count for the coverage, they weigh in the rarity strategy and
// they do not admit a program.
void RunCoverageBitsChecks(Checker* checker) {
  std::string error;
  std::unique_ptr<CoverageMap> map = CoverageMap::Create(9996, &error);
  if (!checker->Check("coverage bits: create a coverage map", map != nullptr,
                      error)) {
    return;
  }
  int fd = shm_open(map->shm_name().c_str(), O_RDWR, 0);
  void* mapping = fd < 0 ? MAP_FAILED
                         : mmap(nullptr, CoverageMap::kShmSize,
                                PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (fd >= 0) {
    close(fd);
  }
  const bool mapped = mapping != MAP_FAILED;
  if (!checker->Check("coverage bits: map the shared memory", mapped,
                      mapped ? "" : strerror(errno))) {
    return;
  }
  // A target with 80 code edges, 10 builtins blocks and 10 optimized code
  // features (inline-bool-flag) writes this header.
  cov::Header header = {};
  header.magic = cov::kMagic;
  header.version = cov::kVersion;
  header.header_size = cov::kHeaderSize;
  header.shm_size = static_cast<uint32_t>(CoverageMap::kShmSize);
  header.mode = static_cast<uint32_t>(cov::Mode::kInlineBoolFlag);
  header.bits_per_edge = 1;
  header.num_edges = 100;
  header.max_edges = cov::MaxEdges(header.shm_size, 1);
  header.num_regions = 3;
  header.regions[0] = {static_cast<uint32_t>(cov::RegionKind::kCode), 1, 80};
  header.regions[1] = {static_cast<uint32_t>(cov::RegionKind::kBuiltins), 81,
                       10};
  header.regions[2] = {static_cast<uint32_t>(cov::RegionKind::kOptimized), 91,
                       10};
  memcpy(mapping, &header, sizeof(header));
  const bool ok = map->FinishInitialization(&error);
  checker->Check("coverage bits: read the header", ok, error);
  if (ok) {
    FeatureWeights::Params params;
    std::unique_ptr<FeatureWeights> weights =
        FeatureWeights::Build(*map, params, {});
    checker->Check(
        "coverage bits: optimized code features",
        !map->counters_mode() && map->num_edges() == 100 &&
            map->counted_edges() == 90 &&
            map->KindOf(95) == CoverageMap::FeatureKind::kOptimized &&
            weights->Of(95) == params.optimized &&
            weights->Of(85) == params.builtins && weights->Of(10) == 1.0 &&
            weights->InFocus(10) && !weights->InFocus(95),
        StringPrintf("counted %u, w(95) %g, w(85) %g, admits 95: %d",
                     map->counted_edges(), weights->Of(95), weights->Of(85),
                     weights->InFocus(95)));
  }
  munmap(mapping, CoverageMap::kShmSize);
}

// The first 5 columns of the row of `signature_text` in signatures.txt
// (signature, crashes, saved, minimized, reproduced), or "".
std::string SignatureRow(const std::string& summary,
                         const std::string& signature_text) {
  const size_t start = summary.find(signature_text + " |");
  if (start == std::string::npos) {
    return "";
  }
  size_t end = start;
  for (int column = 0; column < 5 && end != std::string::npos; column++) {
    end = summary.find(" | ", end + 1);
  }
  return summary.substr(
      start, end == std::string::npos ? std::string::npos : end - start);
}

// Offline: the minimize slots of a crash signature. At most
// `minimize_crashes_per_signature` crashes of one signature hold a minimize
// slot at the same time. A crash whose minimization fails gives back its
// slot. A crash without a slot gives back nothing.
void RunCrashSlotChecks(Checker* checker) {
  const char* name =
      "crash slots: only a crash with a minimize slot gives back a slot";
  char dir_template[] = "/tmp/redline-selftest.XXXXXX";
  if (mkdtemp(dir_template) == nullptr) {
    checker->Check(name, false, StringPrintf("mkdtemp: %s", strerror(errno)));
    return;
  }
  const std::string dir = dir_template;
  const std::string summary_path = dir + "/crashes/signatures.txt";
  std::string error;
  std::string all_succeed, first_fails, summary;
  bool read = false;
  if (MakeDirectories(dir + "/crashes", &error)) {
    FuzzConfig config;
    config.out_dir = dir;
    config.max_crashes_per_signature = 5;
    config.minimize_crashes_per_signature = 2;
    Fuzzer fuzzer(std::move(config));
    // The crashes that got a minimize slot, of 6 crashes of one signature.
    // The minimization fails for the crashes in `failures`.
    auto slots = [&fuzzer](int line, uint64_t first_hash,
                           const std::set<int>& failures) {
      const CrashSignature signature = CrashSignature::FromOutput(
          6, 0,
          StringPrintf("#\n# Fatal error in a.cc, line %d\n# Check failed: "
                       "x.\n",
                       line));
      std::string crashes;
      for (int i = 0; i < 6; i++) {
        if (fuzzer.RecordCrashForTest(first_hash + static_cast<uint64_t>(i),
                                      signature, failures.count(i) == 0)) {
          crashes += StringPrintf("%s%d", crashes.empty() ? "" : " ", i);
        }
      }
      return crashes;
    };
    all_succeed = slots(1, 100, {});
    first_fails = slots(2, 200, {0});
    read = ReadFileToString(summary_path, &summary, &error);
  }
  // The columns: signature | crashes | saved | minimized | reproduced.
  const std::string row1 = SignatureRow(summary, "fatal a.cc:1 x.");
  const std::string row2 = SignatureRow(summary, "fatal a.cc:2 x.");
  const bool ok = read && all_succeed == "0 1" && first_fails == "0 1 2" &&
                  row1 == "fatal a.cc:1 x. | 6 | 5 | 2 | 6" &&
                  row2 == "fatal a.cc:2 x. | 6 | 5 | 2 | 6";
  checker->Check(
      name, ok,
      StringPrintf("slots \"%s\" and \"%s\", rows \"%s\" and \"%s\"%s%s",
                   all_succeed.c_str(), first_fails.c_str(), row1.c_str(),
                   row2.c_str(), error.empty() ? "" : ", ", error.c_str()));
  std::remove(summary_path.c_str());
  rmdir((dir + "/crashes").c_str());
  rmdir(dir.c_str());
}

// Offline: the Fuzzilli shared-memory layout (src/fuzzilli/cov.cc). A
// 4-byte edge count at offset 0 is followed by the edge bitmap at offset 4.
void RunFuzzilliCoverageChecks(Checker* checker) {
  std::string error;
  std::unique_ptr<CoverageMap> map = CoverageMap::Create(9995, &error);
  if (!checker->Check("fuzzilli coverage: create a coverage map",
                      map != nullptr, error)) {
    return;
  }
  int fd = shm_open(map->shm_name().c_str(), O_RDWR, 0);
  void* mapping = fd < 0 ? MAP_FAILED
                         : mmap(nullptr, CoverageMap::kShmSize,
                                PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (fd >= 0) {
    close(fd);
  }
  const bool mapped = mapping != MAP_FAILED;
  if (!checker->Check("fuzzilli coverage: map the shared memory", mapped,
                      mapped ? "" : strerror(errno))) {
    return;
  }
  uint8_t* shm = static_cast<uint8_t*>(mapping);
  memset(shm, 0, CoverageMap::kShmSize);
  const uint32_t num_edges = 120;
  memcpy(shm, &num_edges, sizeof(num_edges));
  bool ok = map->FinishInitialization(&error);
  checker->Check("fuzzilli coverage: detect and read the Fuzzilli layout", ok,
                 error);
  if (ok) {
    checker->Check(
        "fuzzilli coverage: geometry and single code region",
        map->is_fuzzilli() && map->target_kind() == TargetKind::kFuzzilli &&
            map->num_edges() == 120 && map->real_edges() == 120 &&
            map->counted_edges() == 120 &&
            strcmp(map->mode_name(), "fuzzilli") == 0 &&
            map->build_id().empty() && map->regions().size() == 1 &&
            map->regions()[0].kind == CoverageMap::RegionKind::kCode &&
            map->regions()[0].first == 1 && map->regions()[0].count == 120,
        StringPrintf("%u edges, mode %s", map->num_edges(), map->mode_name()));
    // The bitmap starts at offset 4, right after `num_edges`. Edge 1 and edge
    // `num_edges` are valid edges, and `CoverageMap` ignores bit `num_edges +
    // 1`.
    uint8_t* bits = shm + CoverageMap::kFuzzilliHeaderSize;
    for (uint32_t bit :
         {uint32_t{1}, uint32_t{5}, uint32_t{115}, num_edges, num_edges + 1}) {
      bits[bit / 8] |= static_cast<uint8_t>(1u << (bit % 8));
    }
    const std::vector<uint32_t> expected = {1, 5, 115, num_edges};
    checker->Check(
        "fuzzilli coverage: bits after the 4-byte edge count",
        map->CurrentEdges() == expected && map->EvaluateNewEdges() == expected,
        StringPrintf("%zu edges", map->CurrentEdges().size()));
    // ClearBitmap clears the bitmap at offset 4 and keeps num_edges at 0..3.
    map->ClearBitmap();
    uint32_t kept_edges = 0;
    memcpy(&kept_edges, shm, sizeof(kept_edges));
    checker->Check(
        "fuzzilli coverage: ClearBitmap keeps the edge count at offset 0",
        map->CurrentEdges().empty() && kept_edges == 120,
        StringPrintf("num_edges=%u", kept_edges));
    // A respawned process with a different edge count is an error.
    const uint32_t other_edges = 121;
    memcpy(shm, &other_edges, sizeof(other_edges));
    std::string changed;
    checker->Check("fuzzilli coverage: a changed edge count is an error",
                   !map->FinishInitialization(&changed) &&
                       changed.find("mixing binaries") != std::string::npos,
                   changed);
  }
  // --target-kind=redline rejects the Fuzzilli layout, and --targets is
  // rejected in Fuzzilli mode.
  memcpy(shm, &num_edges, sizeof(num_edges));
  std::unique_ptr<CoverageMap> forced_redline =
      CoverageMap::Create(9994, &error);
  if (forced_redline != nullptr) {
    forced_redline->SetTargetKind(TargetKind::kRedline);
    int rfd = shm_open(forced_redline->shm_name().c_str(), O_RDWR, 0);
    if (rfd >= 0) {
      if (pwrite(rfd, &num_edges, sizeof(num_edges), 0) !=
          static_cast<ssize_t>(sizeof(num_edges))) {
        rfd = -1;
      }
      close(rfd);
    }
    std::string reason;
    checker->Check(
        "fuzzilli coverage: --target-kind=redline rejects the Fuzzilli layout",
        rfd >= 0 && !forced_redline->FinishInitialization(&reason), reason);
  }
  std::unique_ptr<CoverageMap> with_targets = CoverageMap::Create(9993, &error);
  if (with_targets != nullptr) {
    CoverageMap::Watch watch;
    watch.edges = {5};
    with_targets->SetWatch(watch);
    int tfd = shm_open(with_targets->shm_name().c_str(), O_RDWR, 0);
    if (tfd >= 0) {
      if (pwrite(tfd, &num_edges, sizeof(num_edges), 0) !=
          static_cast<ssize_t>(sizeof(num_edges))) {
        tfd = -1;
      }
      close(tfd);
    }
    std::string reason;
    checker->Check(
        "fuzzilli coverage: --targets is rejected with a clear message",
        tfd >= 0 && !with_targets->FinishInitialization(&reason) &&
            reason.find("--targets") != std::string::npos,
        reason);
  }
  munmap(mapping, CoverageMap::kShmSize);
}

int RunSelftest(const Flags& flags) {
  Checker checker;
  std::string error;

  // Offline checks first: they need no target.
  RunCrashSignatureChecks(&checker);
  RunReprlBundleChecks(&checker);
  RunCoverageHeaderChecks(&checker);
  RunFuzzilliCoverageChecks(&checker);
  RunD8FlagProfileChecks(&checker);
  RunCoverageCounterChecks(&checker);
  RunCoverageBitsChecks(&checker);
  RunCrashSlotChecks(&checker);
  RunCrashMinimizerChecks(&checker);
  RunFlagMinimizerChecks(&checker);
  RunAmplifiedTestChecks(&checker);
  RunBundleStepChecks(&checker);
  RunNodeKindChecks(&checker);
  RunPresetChecks(&checker);
  std::string testdata = flags.GetString("testdata", DefaultTestdataDir());
  if (testdata.empty()) {
    checker.Check("selection: locate experimental/redline/testdata", false,
                  "not found next to the binary; pass --testdata=DIR");
  } else {
    RunSelectionChecks(&checker, testdata,
                       flags.GetBool("write-golden", false));
  }
  if (flags.GetBool("offline", false)) {
    printf("selftest: %d passed, %d failed\n", checker.passed(),
           checker.failed());
    return checker.failed() == 0 ? kExitOk : kExitFailure;
  }

  TargetOptions target;
  if (!TargetOptions::FromFlags(flags, &target, &error)) {
    REDLINE_ERROR(kLog, "%s", error.c_str());
    return kExitUsage;
  }
  // The respawn behaviour is part of what is tested; use a small value.
  target.max_execs_before_respawn =
      static_cast<int>(flags.GetInt("max-execs-before-respawn", 50));
  int speed_runs = static_cast<int>(flags.GetInt("speed-runs", 200));

  printf("target: %s\n", target.CommandLine().c_str());

  std::unique_ptr<ScriptRunner> runner =
      ScriptRunner::Create(target, 0, &error);
  if (!checker.Check("create runner", runner != nullptr, error)) {
    return kExitFailure;
  }
  if (!checker.Check("REPRL handshake and coverage initialization",
                     runner->Initialize(&error), error)) {
    return kExitFailure;
  }
  checker.Info("instrumented edges",
               StringPrintf("%u", runner->coverage().num_edges()));
  const bool fuzzilli_target = runner->coverage().is_fuzzilli();
  {
    const CoverageMap& coverage = runner->coverage();
    if (fuzzilli_target) {
      checker.Info("coverage header",
                   StringPrintf("fuzzilli layout (%u edges, no region table or "
                                "build id)",
                                coverage.num_edges()));
    } else {
      const cov::Header& header = coverage.header();
      std::string regions;
      for (uint32_t i = 0; i < header.num_regions; i++) {
        static const char* const kKinds[] = {"?", "code", "builtins",
                                             "optimized"};
        const cov::Region& region = header.regions[i];
        regions += StringPrintf(" %s %u..%u",
                                kKinds[region.kind <= 3 ? region.kind : 0],
                                region.first, region.first + region.count - 1);
      }
      checker.Info("coverage header",
                   StringPrintf("%s, %u bit(s) per edge,%s, build id %s",
                                coverage.mode_name(), header.bits_per_edge,
                                regions.c_str(), coverage.build_id().c_str()));
    }
  }

  ScriptRunner::Result result;

  // Basic outcomes.
  if (RunOrFail(&checker, runner.get(), "empty script", "", &result)) {
    checker.Check("empty script succeeds", result.execution.succeeded(),
                  Describe(result.execution));
  }
  if (RunOrFail(&checker, runner.get(), "uncaught exception",
                "throw new Error('expected');", &result)) {
    checker.Check("uncaught exception is reported as failed",
                  result.execution.outcome == Outcome::kFailed,
                  Describe(result.execution));
  }
  if (RunOrFail(&checker, runner.get(), "syntax error", "let let = ;",
                &result)) {
    checker.Check("syntax error is reported as failed",
                  result.execution.outcome == Outcome::kFailed,
                  Describe(result.execution));
  }

  // Output channels.
  {
    const char* print_script =
        fuzzilli_target ? "fuzzilli('FUZZILLI_PRINT', 'hello fuzzout');"
                        : "redline('REDLINE_PRINT', 'hello fuzzout');";
    const char* print_label = fuzzilli_target
                                  ? "fuzzilli('FUZZILLI_PRINT') is captured"
                                  : "redline('REDLINE_PRINT') is captured";
    if (RunOrFail(&checker, runner.get(), "fuzzout", print_script, &result)) {
      checker.Check(print_label,
                    result.execution.succeeded() &&
                        result.execution.fuzzout.find("hello fuzzout") !=
                            std::string::npos,
                    Describe(result.execution) + ", fuzzout=\"" +
                        result.execution.fuzzout + "\"");
    }
  }
  // Environment introspection over the fuzzout channel.
  if (RunOrFail(&checker, runner.get(), "introspection",
                std::string(Environment::IntrospectionScript()), &result)) {
    std::string env_error;
    std::unique_ptr<Environment> env =
        result.execution.succeeded()
            ? Environment::WithDiscovered(result.execution.fuzzout, &env_error)
            : nullptr;
    checker.Check(
        "environment introspection works on the target",
        env != nullptr && !env->discovered().empty(),
        env != nullptr
            ? StringPrintf("%zu globals", env->discovered().size())
            : (env_error.empty() ? Describe(result.execution) : env_error));
  }
  if (RunOrFail(&checker, runner.get(), "stdout",
                "print('hello stdout'); console.log('hello console');",
                &result)) {
    bool ok = result.execution.succeeded() &&
              result.execution.stdout_output.find("hello stdout") !=
                  std::string::npos &&
              result.execution.stdout_output.find("hello console") !=
                  std::string::npos;
    checker.Check("stdout is captured", ok,
                  Describe(result.execution) + ", stdout=\"" +
                      result.execution.stdout_output + "\"");
  }
  if (RunOrFail(&checker, runner.get(), "stderr", "console.error('to stderr')",
                &result)) {
    checker.Check(
        "stderr is captured",
        result.execution.stderr_output.find("to stderr") != std::string::npos,
        "stderr=\"" + result.execution.stderr_output + "\"");
  }
  if (RunOrFail(&checker, runner.get(), "output isolation", "print('second')",
                &result)) {
    checker.Check(
        "output of earlier executions is not leaked",
        result.execution.stdout_output.find("hello") == std::string::npos &&
            result.execution.fuzzout.empty(),
        "stdout=\"" + result.execution.stdout_output + "\"");
  }

  // Timeouts.
  {
    uint64_t timeout_us = static_cast<uint64_t>(300) * 1000;
    Stopwatch stopwatch;
    if (runner->Run("while (true) {}", timeout_us, &result, &error)) {
      uint64_t elapsed = stopwatch.ElapsedUs();
      checker.Check("infinite loop times out",
                    result.execution.outcome == Outcome::kTimedOut &&
                        elapsed >= timeout_us && elapsed < timeout_us * 20,
                    Describe(result.execution) +
                        StringPrintf(", wall %.1f ms",
                                     static_cast<double>(elapsed) / 1000.0));
    } else {
      checker.Check("infinite loop times out", false, error);
    }
    if (RunOrFail(&checker, runner.get(), "after timeout", "1 + 1", &result)) {
      checker.Check("target is respawned after a timeout",
                    result.execution.succeeded(), Describe(result.execution));
    }
  }

  // Crashes.
  struct CrashCase {
    const char* description;
    int kind;
    bool required;
  };
  const CrashCase crash_cases[] = {
      {"IMMEDIATE_CRASH", 0, true},    {"CHECK failure", 1, true},
      {"DCHECK failure", 2, false},    {"wild write", 3, true},
      {"DEBUG build check", 8, false}, {"sandbox violation", 9, false},
  };
  for (const CrashCase& c : crash_cases) {
    const char* crash_fn = fuzzilli_target ? "fuzzilli('FUZZILLI_CRASH'"
                                           : "redline('REDLINE_CRASH'";
    std::string script = StringPrintf("%s, %d)", crash_fn, c.kind);
    std::string name =
        StringPrintf("%s, %d) [%s]", crash_fn, c.kind, c.description);
    if (!RunOrFail(&checker, runner.get(), name.c_str(), script, &result)) {
      continue;
    }
    bool crashed = result.execution.crashed();
    if (c.required) {
      checker.Check(name.c_str(), crashed, Describe(result.execution));
    } else {
      checker.Info(name.c_str(), (crashed ? "crashes" : "does not crash") +
                                     std::string(" -- ") +
                                     Describe(result.execution));
    }
    if (RunOrFail(&checker, runner.get(), "after crash", "1 + 1", &result)) {
      checker.Check("target is respawned after a crash",
                    result.execution.succeeded(), Describe(result.execution));
    }
  }

  // Coverage.
  const char* kProgramA =
      "function f(a) { let s = 0; for (let i = 0; i < a.length; i++) s += "
      "a[i]; return s; }\n"
      "let arr = [1, 2.5, 'x', {}, null];\n"
      "for (let j = 0; j < 100; j++) f(arr);\n"
      "class C { #p = 1; get p() { return this.#p; } }\n"
      "new C().p;\n";
  const char* kProgramB =
      "let m = new Map(); for (let i = 0; i < 50; i++) m.set(i, 'v' + i);\n"
      "let s = new Set([1, 2, 3]); [...m.keys()].filter(x => s.has(x));\n"
      "JSON.parse(JSON.stringify({a: [1, {b: 2}], c: 'd'}));\n"
      "/a(b+)c/.exec('xxabbbcxx'); new Date(0).toISOString();\n";
  std::vector<uint32_t> edges_a;
  if (RunOrFail(&checker, runner.get(), "coverage A", kProgramA, &result)) {
    edges_a = result.new_edges;
    checker.Check("first execution of a program finds new edges",
                  result.execution.succeeded() && !result.new_edges.empty(),
                  StringPrintf("%zu new edges, %u hit", result.new_edges.size(),
                               result.edges_hit));
  }
  if (RunOrFail(&checker, runner.get(), "coverage A again", kProgramA,
                &result)) {
    checker.Check(
        "re-running the same program finds (almost) no new edges",
        result.new_edges.size() * 100 < std::max<size_t>(edges_a.size(), 1),
        StringPrintf("%zu new edges on second run vs %zu on first",
                     result.new_edges.size(), edges_a.size()));
  }
  if (RunOrFail(&checker, runner.get(), "coverage B", kProgramB, &result)) {
    checker.Check("a different program finds new edges",
                  result.execution.succeeded() && !result.new_edges.empty(),
                  StringPrintf("%zu new edges", result.new_edges.size()));
  }
  if (RunOrFail(&checker, runner.get(), "coverage of failing program",
                "let q = []; q.push(1); throw 1;", &result)) {
    checker.Check("failed executions do not advance coverage",
                  result.execution.outcome == Outcome::kFailed &&
                      result.new_edges.empty() && result.edges_hit > 0,
                  StringPrintf("%zu new edges, %u hit", result.new_edges.size(),
                               result.edges_hit));
  }

  // Determinism of edges across runs. V8 is not deterministic at the edge
  // level: GC timing, tiering decisions and concurrent compiler threads make
  // 5-15% of the edges of any script flaky (even `let x = 1 + 2;` shows ~6%).
  // This is the reason for the multi-run determinism check in the fuzzing
  // loop; here we only sanity-check that the stable core is large (a broken
  // coverage map would show up as near-random bitmaps) and report the rate.
  {
    struct Probe {
      const char* name;
      const char* script;
    };
    const Probe kProbes[] = {{"trivial script", "let x = 1 + 2;"},
                             {"program A", kProgramA}};
    for (const Probe& probe : kProbes) {
      std::vector<uint32_t> all_runs;
      std::set<uint32_t> any_run;
      bool ok = true;
      for (int i = 0; i < 5; i++) {
        Execution execution;
        if (!runner->RunWithoutEvaluation(probe.script, target.timeout_us,
                                          &execution, &error)) {
          ok = false;
          break;
        }
        std::vector<uint32_t> current = runner->coverage().CurrentEdges();
        if (i == 0) {
          all_runs = current;
        } else {
          std::vector<uint32_t> intersection;
          std::set_intersection(all_runs.begin(), all_runs.end(),
                                current.begin(), current.end(),
                                std::back_inserter(intersection));
          all_runs.swap(intersection);
        }
        any_run.insert(current.begin(), current.end());
      }
      double stable_pct = any_run.empty()
                              ? 0.0
                              : 100.0 * static_cast<double>(all_runs.size()) /
                                    static_cast<double>(any_run.size());
      checker.Info(
          StringPrintf("edge nondeterminism of %s", probe.name).c_str(),
          StringPrintf("%zu edges in every one of 5 runs, %zu in at "
                       "least one run (%.1f%% stable)",
                       all_runs.size(), any_run.size(), stable_pct));
      checker.Check(
          StringPrintf("stable edge core of %s is >= 75%% of the union",
                       probe.name)
              .c_str(),
          ok && !all_runs.empty() && stable_pct >= 75.0,
          StringPrintf("%.1f%%", stable_pct));
    }
  }

  // Respawning.
  {
    uint64_t spawns_before = runner->child().total_spawns();
    int runs = target.max_execs_before_respawn * 2 + 1;
    bool ok = true;
    for (int i = 0; i < runs; i++) {
      if (!runner->Run("", &result, &error) || !result.execution.succeeded()) {
        ok = false;
        break;
      }
    }
    uint64_t spawns = runner->child().total_spawns() - spawns_before;
    checker.Check("target is respawned every max_execs_before_respawn runs",
                  ok && spawns >= 2,
                  StringPrintf("%llu spawns in %d runs",
                               static_cast<unsigned long long>(spawns), runs));
  }

  // Random flags (randomize_d8_flags): a new target process gets new
  // flags, except while the flags are pinned.
  {
    TargetOptions random_target = target;
    int draws = 0;
    random_target.random_args = [&draws] {
      draws++;
      // Two harmless flags, so that the check can see which draw a target
      // process got.
      return std::vector<std::string>{draws % 2 == 1 ? "--stress-ic"
                                                     : "--no-stress-ic"};
    };
    std::unique_ptr<ScriptRunner> random_runner =
        ScriptRunner::Create(random_target, 1, &error);
    bool ok = random_runner != nullptr && random_runner->Initialize(&error);
    std::vector<std::string> first, second, pinned, released;
    auto respawn = [&](std::vector<std::string>* args) {
      random_runner->child().Kill();
      ok = ok && random_runner->Run("", &result, &error) &&
           result.execution.succeeded();
      *args = random_runner->child().CurrentArgs();
    };
    if (ok) {
      first = random_runner->child().CurrentArgs();
      respawn(&second);
      random_runner->child().Pin(true);
      respawn(&pinned);
      random_runner->child().Pin(false);
      respawn(&released);
    }
    auto starts_with = [](const std::vector<std::string>& args,
                          const char* flag) {
      return !args.empty() && args.front() == flag;
    };
    checker.Check(
        "random flags: a new target gets new flags, a pinned one keeps them",
        ok && starts_with(first, "--stress-ic") &&
            starts_with(second, "--no-stress-ic") && pinned == second &&
            starts_with(released, "--stress-ic") && draws == 3,
        ok ? StringPrintf("%d draws", draws) : error);
    // A respawn while pinned keeps the flags. Then Pin(false) kills the
    // target, because its flags ran max_execs_before_respawn runs, and the
    // next target draws new flags.
    bool respawned = false;
    bool killed = false;
    std::vector<std::string> kept, renewed;
    if (ok) {
      random_runner->child().Pin(true);
      uint64_t spawns_before = random_runner->child().total_spawns();
      for (int i = 0; ok && i < random_target.max_execs_before_respawn; i++) {
        ok = random_runner->Run("", &result, &error) &&
             result.execution.succeeded();
      }
      respawned = random_runner->child().total_spawns() > spawns_before;
      kept = random_runner->child().CurrentArgs();
      random_runner->child().Pin(false);
      killed = !random_runner->child().is_running();
      ok = ok && random_runner->Run("", &result, &error) &&
           result.execution.succeeded();
      renewed = random_runner->child().CurrentArgs();
    }
    checker.Check("random flags: unpinning replaces a target with old flags",
                  ok && respawned && kept == released && killed &&
                      starts_with(renewed, "--no-stress-ic") && draws == 4,
                  ok ? StringPrintf("%d draws", draws) : error);
  }

  // Speed (informational).
  if (speed_runs > 0) {
    Stopwatch stopwatch;
    bool ok = true;
    for (int i = 0; i < speed_runs; i++) {
      if (!runner->Run("let x = 1 + 1;", &result, &error)) {
        ok = false;
        break;
      }
    }
    double seconds = stopwatch.ElapsedSeconds();
    checker.Info(
        "execution speed",
        ok ? StringPrintf("%.0f execs/s for a trivial script (%d runs)",
                          static_cast<double>(speed_runs) / seconds, speed_runs)
           : "failed: " + error);
  }

  // The session of this check has a worker 0. The coverage map of worker 0
  // has the same name as the map of `runner`, so stop `runner` first.
  runner.reset();
  RunCrashReproCheck(&checker, target, fuzzilli_target);
  RunCrashFallbackCheck(&checker, target, fuzzilli_target);
  RunCrashFlagsCheck(&checker, target, fuzzilli_target);
  RunCrashAmplifiedCheck(&checker, target, fuzzilli_target);
  RunCrashBundleCheck(&checker, target, fuzzilli_target);

  printf("selftest: %d passed, %d failed\n", checker.passed(),
         checker.failed());
  return checker.failed() == 0 ? kExitOk : kExitFailure;
}

}  // namespace

const Command kSelftestCommand = {
    "selftest",
    "Check that the execution layer works against a target",
    "usage: redline selftest --d8=PATH [options] | --offline [options]\n"
    "\n"
    "Runs a series of checks against the target: REPRL handshake, outcome\n"
    "classification (success, exception, timeout, crash), output capture,\n"
    "coverage evaluation, edge determinism and respawning. A short session\n"
    "then checks the crash minimizer. Prints PASS/FAIL per check and exits\n"
    "with 1 if any check failed.\n"
    "\n"
    "Before the target checks it runs the offline selection checks: the\n"
    "default seed, partner, mutator and generator strategies must reproduce\n"
    "the picks recorded in experimental/redline/testdata/selection/*.trace.\n"
    "\n"
    "options:\n"
    "  --speed-runs=N     executions for the speed measurement (200)\n"
    "  --offline          only the checks that need no target (--d8 not "
    "needed)\n"
    "  --testdata=DIR     experimental/redline/testdata (default: found next "
    "to the binary)\n"
    "  --write-golden     rewrite the selection traces instead of comparing "
    "them\n"
    "                     (only for an intended change of the default picks)\n",
    RunSelftest,
};

}  // namespace redline
