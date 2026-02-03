// Using runtime feature detection requires atomics. Currently there are no x86 targets
// that support sse but not `AtomicPtr`.

#[cfg(target_arch = "x86")]
use core::arch::x86::{__cpuid, __cpuid_count, _xgetbv, CpuidResult};
#[cfg(target_arch = "x86_64")]
use core::arch::x86_64::{__cpuid, __cpuid_count, _xgetbv, CpuidResult};

use crate::support::feature_detect::{Flags, get_or_init_flags_cache, unique_masks};

/// CPU features that get cached (doesn't correlate to anything on the CPU).
pub mod cpu_flags {
    use super::unique_masks;

    unique_masks! {
        u32,
        SSE3,
        F16C,
        SSE,
        SSE2,
        ERMSB,
        MOVRS,
        FMA,
        FMA4,
        AVX512FP16,
        AVX512BF16,
    }
}

/// Get CPU features, loading from a cache if available.
pub fn get_cpu_features() -> Flags {
    use core::sync::atomic::AtomicU32;
    static CACHE: AtomicU32 = AtomicU32::new(0);
    get_or_init_flags_cache(&CACHE, load_x86_features)
}

/// Read from cpuid and translate to a `Flags` instance, using `cpu_flags`.
///
/// Implementation is taken from [std-detect][std-detect].
///
/// [std-detect]: https://github.com/rust-lang/stdarch/blob/690b3a6334d482874163bd6fcef408e0518febe9/crates/std_detect/src/detect/os/x86.rs#L142
fn load_x86_features() -> Flags {
    let mut value = Flags::empty();

    if cfg!(target_env = "sgx") {
        // doesn't support this because it is untrusted data
        return Flags::empty();
    }

    // Calling `__cpuid`/`__cpuid_count` from here on is safe because the CPU
    // has `cpuid` support.

    // 0. EAX = 0: Basic Information:
    // - EAX returns the "Highest Function Parameter", that is, the maximum leaf
    //   value for subsequent calls of `cpuinfo` in range [0, 0x8000_0000].
    // - The vendor ID is stored in 12 u8 ascii chars, returned in EBX, EDX, and ECX
    //   (in that order)
    let mut vendor_id = [0u8; 12];
    let max_basic_leaf;
    unsafe {
        let CpuidResult { eax, ebx, ecx, edx } = __cpuid(0);
        max_basic_leaf = eax;
        vendor_id[0..4].copy_from_slice(&ebx.to_ne_bytes());
        vendor_id[4..8].copy_from_slice(&edx.to_ne_bytes());
        vendor_id[8..12].copy_from_slice(&ecx.to_ne_bytes());
    }

    if max_basic_leaf < 1 {
        // Earlier Intel 486, CPUID not implemented
        return value;
    }

    // EAX = 1, ECX = 0: Queries "Processor Info and Feature Bits";
    // Contains information about most x86 features.
    let CpuidResult { ecx, edx, .. } = unsafe { __cpuid(0x0000_0001_u32) };
    let proc_info_ecx = Flags::from_bits(ecx);
    let proc_info_edx = Flags::from_bits(edx);

    // EAX = 7: Queries "Extended Features";
    // Contains information about bmi,bmi2, and avx2 support.
    let mut extended_features_ebx = Flags::empty();
    let mut extended_features_edx = Flags::empty();
    let mut extended_features_eax_leaf_1 = Flags::empty();
    if max_basic_leaf >= 7 {
        let CpuidResult { ebx, edx, .. } = unsafe { __cpuid(0x0000_0007_u32) };
        extended_features_ebx = Flags::from_bits(ebx);
        extended_features_edx = Flags::from_bits(edx);

        let CpuidResult { eax, .. } = unsafe { __cpuid_count(0x0000_0007_u32, 0x0000_0001_u32) };
        extended_features_eax_leaf_1 = Flags::from_bits(eax)
    }

    // EAX = 0x8000_0000, ECX = 0: Get Highest Extended Function Supported
    // - EAX returns the max leaf value for extended information, that is,
    //   `cpuid` calls in range [0x8000_0000; u32::MAX]:
    let extended_max_basic_leaf = unsafe { __cpuid(0x8000_0000_u32) }.eax;

    // EAX = 0x8000_0001, ECX=0: Queries "Extended Processor Info and Feature Bits"
    let mut extended_proc_info_ecx = Flags::empty();
    if extended_max_basic_leaf >= 1 {
        let CpuidResult { ecx, .. } = unsafe { __cpuid(0x8000_0001_u32) };
        extended_proc_info_ecx = Flags::from_bits(ecx);
    }

    let mut enable = |regflags: Flags, regbit, flag| {
        if regflags.test_nth(regbit) {
            value.insert(flag);
        }
    };

    enable(proc_info_ecx, 0, cpu_flags::SSE3);
    enable(proc_info_ecx, 29, cpu_flags::F16C);
    enable(proc_info_edx, 25, cpu_flags::SSE);
    enable(proc_info_edx, 26, cpu_flags::SSE2);
    enable(extended_features_ebx, 9, cpu_flags::ERMSB);
    enable(extended_features_eax_leaf_1, 31, cpu_flags::MOVRS);

    // `XSAVE` and `AVX` support:
    let cpu_xsave = proc_info_ecx.test_nth(26);
    if cpu_xsave {
        // 0. Here the CPU supports `XSAVE`.

        // 1. Detect `OSXSAVE`, that is, whether the OS is AVX enabled and
        //    supports saving the state of the AVX/AVX2 vector registers on
        //    context-switches, see:
        //
        // - [intel: is avx enabled?][is_avx_enabled],
        // - [mozilla: sse.cpp][mozilla_sse_cpp].
        //
        // [is_avx_enabled]: https://software.intel.com/en-us/blogs/2011/04/14/is-avx-enabled
        // [mozilla_sse_cpp]: https://hg.mozilla.org/mozilla-central/file/64bab5cbb9b6/mozglue/build/SSE.cpp#l190
        let cpu_osxsave = proc_info_ecx.test_nth(27);

        if cpu_osxsave {
            // 2. The OS must have signaled the CPU that it supports saving and
            // restoring the:
            //
            // * SSE -> `XCR0.SSE[1]`
            // * AVX -> `XCR0.AVX[2]`
            // * AVX-512 -> `XCR0.AVX-512[7:5]`.
            // * AMX -> `XCR0.AMX[18:17]`
            //
            // by setting the corresponding bits of `XCR0` to `1`.
            //
            // This is safe because the CPU supports `xsave` and the OS has set `osxsave`.
            let xcr0 = unsafe { _xgetbv(0) };
            // Test `XCR0.SSE[1]` and `XCR0.AVX[2]` with the mask `0b110 == 6`:
            let os_avx_support = xcr0 & 6 == 6;
            // Test `XCR0.AVX-512[7:5]` with the mask `0b1110_0000 == 0xe0`:
            let os_avx512_support = xcr0 & 0xe0 == 0xe0;

            // Only if the OS and the CPU support saving/restoring the AVX
            // registers we enable `xsave` support:
            if os_avx_support {
                // See "13.3 ENABLING THE XSAVE FEATURE SET AND XSAVE-ENABLED
                // FEATURES" in the "Intel® 64 and IA-32 Architectures Software
                // Developer’s Manual, Volume 1: Basic Architecture":
                //
                // "Software enables the XSAVE feature set by setting
                // CR4.OSXSAVE[bit 18] to 1 (e.g., with the MOV to CR4
                // instruction). If this bit is 0, execution of any of XGETBV,
                // XRSTOR, XRSTORS, XSAVE, XSAVEC, XSAVEOPT, XSAVES, and XSETBV
                // causes an invalid-opcode exception (#UD)"

                // FMA (uses 256-bit wide registers):
                enable(proc_info_ecx, 12, cpu_flags::FMA);

                // For AVX-512 the OS also needs to support saving/restoring
                // the extended state, only then we enable AVX-512 support:
                if os_avx512_support {
                    enable(extended_features_edx, 23, cpu_flags::AVX512FP16);
                    enable(extended_features_eax_leaf_1, 5, cpu_flags::AVX512BF16);
                }
            }
        }
    }

    // As Hygon Dhyana originates from AMD technology and shares most of the architecture with
    // AMD's family 17h, but with different CPU Vendor ID("HygonGenuine")/Family series number
    // (Family 18h).
    //
    // For CPUID feature bits, Hygon Dhyana(family 18h) share the same definition with AMD
    // family 17h.
    //
    // Related AMD CPUID specification is https://www.amd.com/system/files/TechDocs/25481.pdf
    // (AMD64 Architecture Programmer's Manual, Appendix E).
    // Related Hygon kernel patch can be found on
    // http://lkml.kernel.org/r/5ce86123a7b9dad925ac583d88d2f921040e859b.1538583282.git.puwen@hygon.cn
    if vendor_id == *b"AuthenticAMD" || vendor_id == *b"HygonGenuine" {
        // These features are available on AMD arch CPUs:
        enable(extended_proc_info_ecx, 16, cpu_flags::FMA4);
    }

    value
}

#[cfg(test)]
mod tests {
    extern crate std;
    use std::is_x86_feature_detected;

    use super::*;

    #[test]
    fn check_matches_std() {
        let features = get_cpu_features();
        for i in 0..cpu_flags::ALL.len() {
            let flag = cpu_flags::ALL[i];
            let name = cpu_flags::NAMES[i];

            let std_detected = match flag {
                cpu_flags::SSE3 => is_x86_feature_detected!("sse3"),
                cpu_flags::F16C => is_x86_feature_detected!("f16c"),
                cpu_flags::SSE => is_x86_feature_detected!("sse"),
                cpu_flags::SSE2 => is_x86_feature_detected!("sse2"),
                cpu_flags::ERMSB => is_x86_feature_detected!("ermsb"),
                cpu_flags::MOVRS => continue, // only very recent support in std
                cpu_flags::FMA => is_x86_feature_detected!("fma"),
                cpu_flags::FMA4 => continue, // not yet supported in std
                cpu_flags::AVX512FP16 => is_x86_feature_detected!("avx512fp16"),
                cpu_flags::AVX512BF16 => is_x86_feature_detected!("avx512bf16"),
                _ => panic!("untested CPU flag {name}"),
            };

            assert_eq!(
                std_detected,
                features.contains(flag),
                "different flag {name}. flags: {features:?}"
            );
        }
    }
}
