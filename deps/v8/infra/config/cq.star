# Copyright 2020 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//lib/builders.star", "default_location_filters")
load("//lib/lib.star", "CQ_MODE")

luci.cq(
    submit_max_burst = 1,
    submit_burst_delay = 60 * time.second,
    status_host = "chromium-cq-status.appspot.com",
)

luci.cq_group(
    name = "infra-cq",
    watch = cq.refset(
        repo = "https://chromium.googlesource.com/v8/v8",
        refs = ["refs/heads/infra/config"],
    ),
    retry_config = cq.retry_config(
        single_quota = 2,
        global_quota = 4,
        failure_weight = 2,
        transient_failure_weight = 1,
        timeout_weight = 4,
    ),
    verifiers = [luci.cq_tryjob_verifier(
        "try/v8_presubmit",
        disable_reuse = True,
        cancel_stale = False,
    )],
)

luci.cq_group(
    name = "v8-cq",
    watch = cq.refset(
        repo = "https://chromium.googlesource.com/v8/v8",
        refs = [
            "refs/heads/main",
        ],
    ),
    additional_modes = [
        cq.run_mode(
            name = CQ_MODE.MEGA_CQ_DRY_RUN_NAME,
            cq_label_value = 1,
            triggering_label = "Mega-CQ",
            triggering_value = 1,
        ),
        cq.run_mode(
            name = CQ_MODE.MEGA_CQ_FULL_RUN_NAME,
            cq_label_value = 2,
            triggering_label = "Mega-CQ",
            triggering_value = 1,
        ),
    ],
    tree_status_host = "v8-status.appspot.com",
    retry_config = cq.retry_config(
        single_quota = 2,
        global_quota = 4,
        failure_weight = 2,
        transient_failure_weight = 1,
        timeout_weight = 4,
    ),
    verifiers = [
        luci.cq_tryjob_verifier(
            "chromium:try/linux-blink-rel",
            location_filters = [
                cq.location_filter(path_regexp = "include/cppgc/.+"),
                cq.location_filter(path_regexp = "src/inspector/.+"),
                cq.location_filter(path_regexp = "src/wasm/wasm-feature-flags\\.h"),
                cq.location_filter(path_regexp = "src/wasm/wasm-js\\.cc"),
                cq.location_filter(path_regexp = "src/wasm/wasm-js\\.h"),
                cq.location_filter(path_regexp = "test/inspector/.+"),
                cq.location_filter(path_regexp = "test/mjsunit/wasm/js-api\\.js"),
                cq.location_filter(path_regexp = "test/wasm-js/.+"),
            ],
        ),
        luci.cq_tryjob_verifier(
            "chromium:try/linux-rel",
            location_filters = [
                cq.location_filter(path_regexp = "include/.+\\.h"),
                cq.location_filter(path_regexp = "src/api/api\\.cc"),
                cq.location_filter(path_regexp = "src/inspector/.+"),
                cq.location_filter(path_regexp = "src/common/message-template\\.h"),
                cq.location_filter(path_regexp = "test/inspector/.+"),
            ],
        ),
        luci.cq_tryjob_verifier(
            "node-ci:try/node_ci_linux64_rel",
            cancel_stale = False,
            location_filters = default_location_filters(),
        ),
        luci.cq_tryjob_verifier(
            "v8:try/v8_mega_cq_launcher",
            mode_allowlist = [CQ_MODE.MEGA_CQ_DRY_RUN_NAME, CQ_MODE.MEGA_CQ_FULL_RUN_NAME],
        ),
    ],
)

luci.cq_group(
    name = "v8-branch-cq",
    watch = cq.refset(
        repo = "https://chromium.googlesource.com/v8/v8",
        refs = [
            "refs/branch-heads/.+",
        ],
    ),
    retry_config = cq.retry_config(
        single_quota = 2,
        global_quota = 4,
        failure_weight = 2,
        transient_failure_weight = 1,
        timeout_weight = 4,
    ),
)

luci.cq_group(
    name = "v8-chromium-branch-cq",
    watch = cq.refset(
        repo = "https://chromium.googlesource.com/v8/v8",
        refs = [
            "refs/heads/chromium/.+",
        ],
    ),
    retry_config = cq.retry_config(
        single_quota = 2,
        global_quota = 4,
        failure_weight = 2,
        transient_failure_weight = 1,
        timeout_weight = 4,
    ),
)
