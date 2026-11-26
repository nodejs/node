// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Pinned copy of `v8ProcessArgs` from Fuzzilli
// `Sources/Fuzzilli/Profiles/V8CommonProfile.swift` at commit `7eb96565`.

// Configure V8 invocation arguments. `forSandbox` is used by the V8SandboxProfile. As the sandbox
// fuzzer does not crash on regular assertions, most validation flags do not make sense in that
// configuraiton.
public func v8ProcessArgs(config: ProcessArgsConfig, forSandbox: Bool) -> [String] {
    var args = [
        "--expose-gc",
        "--expose-externalize-string",
        "--omit-quit",
        "--allow-natives-syntax",
        "--fuzzing",
        "--harmony",
        "--experimental-fuzzing",
        "--js-staging",
        "--wasm-staging",
        "--wasm-fast-api",
        "--expose-fast-api",
        "--wasm-test-streaming",  // WebAssembly.compileStreaming & WebAssembly.instantiateStreaming()
    ]
    if forSandbox {
        args.append("--sandbox-fuzzing")
        // This is so that we get an ASan splat directly in the reproducer file.
        args.append("--disable-in-process-stack-traces")
    }

    guard config.randomize else { return args }

    func chooseBooleanFlagRandomly(_ flag: String) {
        assert(!flag.starts(with: "--"))
        args.append(probability(0.5) ? "--\(flag)" : "--no-\(flag)")
    }

    // Prioritize fuzzing "future" logic as it is more likely to have bugs but keep some basic
    // coverage for the shipping non-future logic.
    if probability(0.8) {
        args.append("--future")
    }

    //
    // Existing features that should sometimes be disabled.
    //
    if probability(0.1) {
        args.append("--no-turbofan")
        if probability(0.5) {
            args.append("--maglev-as-top-tier")
        }
    } else if probability(0.1) {
        args.append(
            probability(0.5)
                ? "--turbo-instruction-scheduling"
                : "--turbo-stress-instruction-scheduling")
    }

    if probability(0.1) {
        args.append("--no-flush-bytecode")
    }

    if !args.contains("--maglev-as-top-tier") && probability(0.1) {
        args.append("--no-maglev")
    }

    if probability(0.1) {
        args.append("--no-sparkplug")
    }

    if probability(0.1) {
        args.append("--no-short-builtin-calls")
    }

    if probability(0.8) {
        args.append("--jit-fuzzing")
    }

    // Disabling Liftoff enables "direct" coverage for the optimizing compiler, though some
    // features (like speculative inlining) require a combination of Liftoff and Turbofan.
    // Note that this flag only affects WebAssembly.
    if probability(0.5) {
        args.append("--no-liftoff")
        if probability(0.3) && !forSandbox {
            args.append("--wasm-assert-types")
        }
    } else if probability(0.2) {
        args.append("--wasm-sync-tier-up")
    }

    // This greatly helps the fuzzer to decide inlining wasm functions into each other when
    // %WasmTierUpFunction() is used as in most cases the call counts will be way too low to
    // align with V8's current inlining heuristics (which uses absolute call counts as a
    // deciding factor).
    if probability(0.5) {
        args.append("--wasm-inlining-ignore-call-counts")
    }

    // Force compiled Wasm wrappers.
    if probability(0.3) {
        args.append("--no-wasm-generic-wrapper")
    }

    if probability(0.2) {
        args.append("--no-wasm-native-module-cache")
    }

    if probability(0.2) {
        args.append("--wasm-caching-threshold=\(Int.random(in: 1...1000))")
    }

    if probability(0.2) {
        args.append("--wasm-random-rescheduling")
    }

    if probability(0.4) {
        // This flag tells the engine it should assume that partial OOB writes do not write
        // anything on this architecture and trap immediately, which allows using implicit bounds
        // checks for multi-byte Wasm linear memory stores on arm64 (and for arm64, this is
        // otherwise only enabled on MacOS hardware by default).
        args.append("--wasm-partial-oob-writes-are-noops")
    }

    // Disabling batching allows the fuzzer to reach higher JIT tiers faster; sometimes test the
    // production configuration too.
    if probability(0.8) {
        args.append("--no-baseline-batch-compilation")
    }

    // Disabling lazy feedback allocation allows the fuzzer to reach higher JIT tiers faster;
    // sometimes test with the production configuration too.
    if probability(0.8) {
        args.append("--no-lazy-feedback-allocation")
    }

    //
    // Future features that should sometimes be enabled.
    //
    if probability(0.1) {
        args.append("--minor-ms")
    }

    // Enable the shared heap.
    if probability(0.25) {
        args.append("--shared-strings")
    }

    if !args.contains("--no-maglev") {
        if probability(0.1) {
            args.append("--no-maglev-untagged-phis")
        }
        if probability(0.1) {
            args.append("--no-maglev-loop-peeling")
        }
        if probability(0.25) {
            args.append("--maglev-future")
        }
        if probability(0.1) {
            args.append("--maglev-assert")
        }
        if probability(0.05) {
            args.append("--maglev-assert-stack-size")
        }
        if probability(0.2) {
            args.append("--maglev-non-eager-inlining")
            if probability(0.35) {
                args.append("--max-maglev-eager-inlined-bytecode-size=0")
            }
            if probability(0.4) {  // TODO: @tacet decrease this probability to max 0.2
                args.append("--max-maglev-inlined-bytecode-size-small=0")
            }
        }
        if !args.contains("--no-turbofan") && probability(0.1) {
            args.append("--always-osr-from-maglev")
        }
    }

    if !args.contains("--no-turbofan") && probability(0.7) {
        args.append("--turbolev")
        if probability(0.82) {
            args.append("--turbolev-future")
            if probability(0.3) {  // TODO: @tacet change to 0.15
                args.append("--max-inlined-bytecode-size-small=0")
            }
        }
        if probability(0.1) {
            args.append("--maglev-disable-builtin-reducers")
        }
        if probability(0.1) {
            args.append("--no-turbolev-untagged-phis")
        }
        if probability(0.1) {
            args.append("--no-turbolev-truncated-int32-phis")
        }
        if probability(0.1) {
            args.append("--no-turbolev-non-eager-inlining")
        }
        if probability(0.1) {
            args.append("--no-turbolev-non-eager-loop-peeling")
        }
        if probability(0.1) {
            args.append("--no-turbolev-eager-loop-peeling-osr")
        }
        if probability(0.1) {
            let maxInlinedSize = Int.random(in: 0...920)
            args.append("--max-inlined-bytecode-size=\(maxInlinedSize)")
            args.append(
                "--max-inlined-bytecode-size-cumulative=\(maxInlinedSize + Int.random(in: 0...920))"
            )
        }
        if !args.contains("--max-inlined-bytecode-size-small=0") && probability(0.1) {
            args.append("--max-inlined-bytecode-size-small=\(Int.random(in: 0...54))")
        }
        if probability(0.1) {
            args.append(
                "--max-inlined-bytecode-size-small-with-heapnum-in-out=\(Int.random(in: 0...150))")
        }
    }

    // Optimized code coverage is only produced by Maglev and Turbolev (not by TurboFan).
    let maglevEnabled = !args.contains("--no-maglev")
    let turbolevEnabled = args.contains("--turbolev")
    if !forSandbox && (maglevEnabled || turbolevEnabled)
        && probability(config.isWasmEnabled ? 0.1 : 0.2)
    {
        args.append("--fuzzilli-enable-optimized-code-coverage")
    }

    if probability(0.1) {
        args.append("--efficiency-mode")
    }

    if probability(0.1) {
        args.append("--battery-saver-mode")
    }

    if probability(0.1) {
        args.append("--stress-scavenger-conservative-object-pinning-random")
    }

    if probability(0.1) {
        args.append("--precise-object-pinning")
    }

    if probability(0.1) {
        args.append("--scavenger-chaos-mode")
        let threshold = Int.random(in: 0...100)
        args.append("--scavenger-chaos-mode-threshold=\(threshold)")
    }

    if probability(0.1) {
        args.append("--stress-descriptor-array-trimming")
    }

    if probability(0.1) {
        let stackSize = Int.random(in: 54...863)
        args.append("--stack-size=\(stackSize)")
    }

    // Temporarily enable the three flags below with high probability to
    // stress-test JSPI.
    // Lower the probabilities once we have enough coverage.
    if probability(0.5) {
        let stackSwitchingSize = Int.random(in: 1...300)
        args.append("--wasm-stack-switching-stack-size=\(stackSwitchingSize)")
    }
    if probability(0.5) {
        args.append("--wasm-growable-stacks")
    }
    if probability(0.5) {
        args.append("--stress-wasm-stack-switching")
    }

    if config.enableCustomDescriptors {
        chooseBooleanFlagRandomly("wasm-merged-descriptors")
    }

    if probability(0.5) {
        args.append("--proto-assign-seq-lazy-func-opt")
        if probability(0.5) {
            args.append("--proto-assign-seq-opt-count=1")
        }
    }

    if probability(0.5) {
        args.append("--private-field-bytecodes")
    }

    // Choose the bytecode verification level: default (currently none), light or full.
    if probability(0.67) {
        args.append(probability(0.5) ? "--verify-bytecode-light" : "--verify-bytecode-full")
    }

    if probability(0.1) {
        args.append("--disable-loop-stack-checks")
    }

    //
    // Sometimes enable additional verification/stressing logic (which may be fairly expensive).
    //
    if !forSandbox {
        if probability(0.1) {
            args.append("--verify-heap")
        }
        if probability(0.1) {
            args.append("--turbo-verify")
        }
        if probability(0.1) {
            args.append("--turbo-verify-allocation")
        }
        if probability(0.1) {
            args.append("--assert-types")
        }
        if (!args.contains("--no-maglev") || args.contains("--turbolev")) && probability(0.1) {
            // TODO(tacet): update the Turbolev conditions to !args.contains("--no-turbolev") after Turbolev trial
            args.append("--maglev-assert-types")
        }
        if args.contains("--turbolev") && probability(0.2) {
            args.append("--maglev-range-verification")
        }
        if probability(0.2) {
            args.append("--verify-turboshaft")
        }
        if probability(0.2) {
            args.append("--turboshaft-verify-load-elimination")
        }
        if probability(0.1) {
            args.append("--turboshaft-verify-reductions")
        }
        if probability(0.2) {
            args.append("--turboshaft-verify-load-store-taggedness")
        }
        // This stressing is only reasonable for non-sandbox fuzzing, since it might trigger benign
        // CHECKs that could mask actual sandbox violations. The stressing also happens to break
        // some reproducers because of slightly changing memory layouts, hence this "medium"
        // probability.
        if probability(0.5) {
            args.append("--stress-lazy-source-positions")
        }
        // This stressing is usually useful, but also run with the production configuration
        // sometimes too. Similarly to the above, in sandbox fuzzers it'd trigger benign CHECKs
        // hence avoided.
        if probability(0.9) {
            args.append("--stress-lazy")
        }
    }

    if probability(0.1) {
        args.append(
            "--deopt-every-n-times=\(chooseUniform(from: [100, 250, 500, 1000, 2500, 5000, 10000]))"
        )
    }
    if probability(0.1) {
        args.append("--stress-ic")
    } else if probability(0.1) {
        args.append("--no-use-ic")
    }
    if probability(0.1) {
        args.append("--invocation-count-for-feedback-allocation=\(Int.random(in: 0...8))")
    }
    if probability(0.1) {
        args.append("--no-additive-safe-int-feedback")
    }
    if probability(0.1) {
        args.append("--optimize-on-next-call-optimizes-to-maglev")
    }
    if !args.contains("--no-turbofan") && probability(0.2) {
        args.append("--optimize-maglev-optimizes-to-turbofan")
    }
    if !args.contains("--no-turbofan") && !args.contains("--assert-types") {
        if probability(0.25) {
            args.append("--stress-concurrent-inlining")
            if probability(0.2) {
                args.append("--stress-concurrent-inlining-attach-code")
            }
        }
    }

    //
    // A gc-stress session with some fairly expensive flags.
    //
    if probability(0.1) {
        if probability(0.4) {
            args.append("--stress-marking=\(Int.random(in: 1...100))")
        }
        if probability(0.4) {
            args.append("--stress-scavenge=\(Int.random(in: 1...100))")
        }
        if probability(0.5) {
            args.append("--stress-flush-code")
        }
        if probability(0.5) {
            args.append("--wasm-code-gc")
            args.append("--stress-wasm-code-gc")
        }
        if probability(0.5) {
            args.append("--stress-wasm-memory-moving")
        }
        if probability(0.4) {
            args.append(
                chooseUniform(
                    from: [
                        "--gc-interval=\(Int.random(in: 100...10000))",
                        "--random-gc-interval=\(Int.random(in: 1000...10000))",
                    ]))
        }
        if probability(0.4) {
            args.append("--concurrent-recompilation-queue-length=\(Int.random(in: 4...64))")
            args.append("--concurrent-recompilation-delay=\(Int.random(in: 1...500))")
        }
        if probability(0.6) {
            args.append(
                chooseUniform(
                    from: ["--stress-compaction", "--stress-compaction-random"]))
        }
    }

    //
    // More exotic configuration changes.
    //
    if probability(0.05) {
        if probability(0.5) { args.append("--stress-gc-during-compilation") }
        if probability(0.5) { args.append("--lazy-new-space-shrinking") }
        if probability(0.5) { args.append("--stress-wasm-memory-moving") }
        if probability(0.5) { args.append("--stress-background-compile") }

        chooseBooleanFlagRandomly("always-sparkplug")
        chooseBooleanFlagRandomly("always-osr")
        chooseBooleanFlagRandomly("concurrent-osr")
        chooseBooleanFlagRandomly("force-slow-path")
        chooseBooleanFlagRandomly("lazy")
        chooseBooleanFlagRandomly("lazy-eval")

        // Maglev related flags
        chooseBooleanFlagRandomly("maglev-inline-api-calls")
        chooseBooleanFlagRandomly("maglev-inlining")
        chooseBooleanFlagRandomly("maglev-optimistic-peeled-loops")
        chooseBooleanFlagRandomly("maglev-poly-calls")
        chooseBooleanFlagRandomly("maglev-truncation")
        chooseBooleanFlagRandomly("maglev-cse")
        chooseBooleanFlagRandomly("maglev-range-analysis")
        chooseBooleanFlagRandomly("maglev-escape-analysis")

        // Compiler related flags
        chooseBooleanFlagRandomly("turbo-move-optimization")
        chooseBooleanFlagRandomly("turbo-jt")  // jump threading
        chooseBooleanFlagRandomly("turbo-loop-peeling")
        chooseBooleanFlagRandomly("turbo-loop-variable")
        chooseBooleanFlagRandomly("turbo-loop-rotation")
        chooseBooleanFlagRandomly("turbo-cf-optimization")
        chooseBooleanFlagRandomly("turbo-escape")
        chooseBooleanFlagRandomly("turbo-allocation-folding")
        chooseBooleanFlagRandomly("turbo-instruction-scheduling")
        chooseBooleanFlagRandomly("turbo-stress-instruction-scheduling")
        chooseBooleanFlagRandomly("turbo-store-elimination")
        chooseBooleanFlagRandomly("turbo-rewrite-far-jumps")
        chooseBooleanFlagRandomly("turbo-optimize-apply")
        chooseBooleanFlagRandomly("turbo-load-elimination")
        chooseBooleanFlagRandomly("turbo-inlining")
        args.append(
            chooseUniform(from: [
                "--no-enable-sse3", "--no-enable-ssse3", "--no-enable-sse4-1", "--no-enable-sse4-2",
                "--no-enable-avx", "--no-enable-avx2", "--no-enable-sahf", "--no-enable-fma3",
                "--no-enable-bmi1", "--no-enable-bmi2", "--no-enable-lzcnt", "--no-enable-popcnt",
            ]))

        chooseBooleanFlagRandomly("turboshaft-loop-unrolling")
        chooseBooleanFlagRandomly("turboshaft-load-elimination")
        chooseBooleanFlagRandomly("turboshaft-string-concat-escape-analysis")

        // Wasm related flags
        chooseBooleanFlagRandomly("wasm-loop-peeling")
        chooseBooleanFlagRandomly("turboshaft-wasm-load-elimination")
        chooseBooleanFlagRandomly("wasm-loop-unrolling")
        chooseBooleanFlagRandomly("wasm-inlining")
        chooseBooleanFlagRandomly("wasm-opt")
        chooseBooleanFlagRandomly("wasm-deopt")
        chooseBooleanFlagRandomly("wasm-enforce-bounds-checks")
        chooseBooleanFlagRandomly("wasm-math-intrinsics")
        chooseBooleanFlagRandomly("wasm-bulkmem-inlining")
        chooseBooleanFlagRandomly("wasm-lazy-compilation")
    }

    return args
}
