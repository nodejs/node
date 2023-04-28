# Copyright 2022 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from testrunner.local import utils

# Increase the timeout for these:
SLOW_ARCHS = [
    "arm", "arm64", "mips64", "mips64el", "s390", "s390x", "riscv32", "riscv64",
    "loong64"
]


class BuildConfig(object):

  def __init__(self, build_config, options):
    self.options = options
    # In V8 land, GN's x86 is called ia32.
    if build_config['v8_target_cpu'] == 'x86':
      self.arch = 'ia32'
    else:
      self.arch = build_config['v8_target_cpu']

    self.asan = build_config['is_asan']
    self.cfi_vptr = build_config['is_cfi']
    self.code_comments = build_config['v8_code_comments']
    self.component_build = build_config['is_component_build']
    self.concurrent_marking = build_config['v8_enable_concurrent_marking']
    self.conservative_stack_scanning = build_config[
        'v8_enable_conservative_stack_scanning']
    self.control_flow_integrity = build_config['v8_control_flow_integrity']
    self.dcheck_always_on = build_config['dcheck_always_on']
    self.debug_code = build_config['v8_enable_debug_code']
    self.dict_property_const_tracking = build_config[
        'v8_dict_property_const_tracking']
    self.disassembler = build_config['v8_enable_disassembler']
    self.gdbjit = build_config['v8_enable_gdbjit']
    self.is_android = build_config['is_android']
    self.is_clang = build_config['is_clang']
    self.is_clang_coverage = build_config['is_clang_coverage']
    self.is_debug = build_config['is_debug']
    self.is_DEBUG_defined = build_config['is_DEBUG_defined']
    self.is_full_debug = build_config['is_full_debug']
    self.lite_mode = build_config['v8_enable_lite_mode']
    self.maglev = build_config['v8_enable_maglev']
    self.msan = build_config['is_msan']
    self.no_i18n = not build_config['v8_enable_i18n_support']
    self.pointer_compression = build_config['v8_enable_pointer_compression']
    self.pointer_compression_shared_cage = build_config[
        'v8_enable_pointer_compression_shared_cage']
    self.predictable = build_config['v8_enable_verify_predictable']
    self.sandbox = build_config['v8_enable_sandbox']
    self.shared_ro_heap = build_config['v8_enable_shared_ro_heap']
    self.simulator_run = (
        build_config['target_cpu'] != build_config['v8_target_cpu'])
    self.single_generation = build_config['v8_enable_single_generation']
    self.slow_dchecks = build_config['v8_enable_slow_dchecks']
    self.third_party_heap = build_config['v8_enable_third_party_heap']
    self.tsan = build_config['is_tsan']
    self.turbofan = build_config['v8_enable_turbofan']
    # TODO(machenbach): We only have ubsan not ubsan_vptr.
    self.ubsan_vptr = build_config['is_ubsan_vptr']
    self.verify_csa = build_config['v8_enable_verify_csa']
    self.verify_heap = build_config['v8_enable_verify_heap']
    self.webassembly = build_config['v8_enable_webassembly']
    self.write_barriers = not build_config['v8_disable_write_barriers']
    # TODO(jgruber): Don't rename once it's no longer necessary to avoid
    # conflicts with test variant names.
    self.jitless_build_mode = build_config['v8_jitless']
    # Export only for MIPS target
    if self.arch in ['mips64', 'mips64el']:
      self._mips_arch_variant = build_config['mips_arch_variant']
      self.mips_use_msa = build_config['mips_use_msa']

  @property
  def use_sanitizer(self):
    return (self.asan or self.cfi_vptr or self.msan or self.tsan or
            self.ubsan_vptr)

  @property
  def no_js_shared_memory(self):
    return (not self.shared_ro_heap) or (
        self.pointer_compression and
        not self.pointer_compression_shared_cage) or (not self.write_barriers)

  @property
  def is_mips_arch(self):
    return self.arch in ['mips64', 'mips64el']

  @property
  def simd_mips(self):
    return (self.is_mips_arch and self._mips_arch_variant == "r6" and
            self.mips_use_msa)

  @property
  def mips_arch_variant(self):
    return (self.is_mips_arch and self._mips_arch_variant)

  @property
  def no_simd_hardware(self):
    # TODO(liviurau): Add some tests and refactor the logic here.
    # We try to find all the reasons why we have no_simd.
    no_simd_hardware = any(i in self.options.extra_flags for i in [
        '--noenable-sse3', '--no-enable-sse3', '--noenable-ssse3',
        '--no-enable-ssse3', '--noenable-sse4-1', '--no-enable-sse4_1'
    ])

    # Set no_simd_hardware on architectures without Simd enabled.
    if self.arch == 'mips64el':
      no_simd_hardware = not self.simd_mips

    if self.arch == 'loong64'  or \
       self.arch == 'riscv32':
      no_simd_hardware = True

    # S390 hosts without VEF1 do not support Simd.
    if self.arch == 's390x' and \
       not self.simulator_run and \
       not utils.IsS390SimdSupported():
      no_simd_hardware = True

    # Ppc64 processors earlier than POWER9 do not support Simd instructions
    if self.arch == 'ppc64' and \
       not self.simulator_run and \
       utils.GuessPowerProcessorVersion() < 9:
      no_simd_hardware = True

    return no_simd_hardware

  def timeout_scalefactor(self, initial_factor):
    """Increases timeout for slow build configurations."""
    factors = dict(
        lite_mode=2,
        predictable=4,
        tsan=2,
        use_sanitizer=1.5,
        is_full_debug=4,
    )
    result = initial_factor
    for k, v in factors.items():
      if getattr(self, k, False):
        result *= v
    if self.arch in SLOW_ARCHS:
      result *= 4.5
    return result

  def __str__(self):
    attrs = [
        'asan',
        'cfi_vptr',
        'code_comments',
        'control_flow_integrity',
        'dcheck_always_on',
        'debug_code',
        'dict_property_const_tracking',
        'disassembler',
        'gdbjit',
        'is_debug',
        'is_DEBUG_defined',
        'jitless_build_mode',
        'lite_mode',
        'maglev',
        'msan',
        'no_i18n',
        'pointer_compression',
        'pointer_compression_shared_cage',
        'predictable',
        'sandbox',
        'slow_dchecks',
        'third_party_heap',
        'tsan',
        'turbofan',
        'ubsan_vptr',
        'verify_csa',
        'verify_heap',
        'webassembly',
    ]
    detected_options = [attr for attr in attrs if getattr(self, attr, False)]
    return ', '.join(detected_options)
