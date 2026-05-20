{
  'conditions': [
    ['target_arch=="ppc64" and OS in ("aix", "os400")', {
      'includes': ['config/archs/aix64-gcc-as/no-asm/openssl-cl.gypi'],
    }, 'target_arch=="ppc64" and OS=="linux" and node_byteorder =="little"', {
      'includes': ['config/archs/linux-ppc64le/no-asm/openssl-cl.gypi'],
    }, 'target_arch=="s390x" and OS=="linux"', {
      'includes': ['config/archs/linux64-s390x/no-asm/openssl-cl.gypi'],
    }, 'target_arch=="arm" and OS in ("linux", "android")', {
      'includes': ['config/archs/linux-armv4/no-asm/openssl-cl.gypi'],
    }, 'target_arch=="arm64" and OS in ("linux", "android", "openharmony")', {
      'includes': ['config/archs/linux-aarch64/no-asm/openssl-cl.gypi'],
    }, 'target_arch=="arm64" and OS=="win"', {
      'includes': ['config/archs/VC-WIN64-ARM/no-asm/openssl-cl.gypi'],
    }, 'target_arch=="ia32" and OS=="freebsd"', {
      'includes': ['config/archs/BSD-x86/no-asm/openssl-cl.gypi'],
    }, 'target_arch=="ia32" and OS=="linux"', {
      'includes': ['config/archs/linux-elf/no-asm/openssl-cl.gypi'],
    }, 'target_arch=="ia32" and OS=="mac"', {
      'includes': ['config/archs/darwin-i386-cc/no-asm/openssl-cl.gypi'],
    }, 'target_arch=="ia32" and OS=="solaris"', {
      'includes': ['config/archs/solaris-x86-gcc/no-asm/openssl-cl.gypi'],
    }, 'target_arch=="ia32" and OS=="win"', {
      'includes': ['config/archs/VC-WIN32/no-asm/openssl-cl.gypi'],
    }, 'target_arch=="ia32"', {
      # noasm linux-elf for other ia32 platforms
      'includes': ['config/archs/linux-elf/no-asm/openssl-cl.gypi'],
    }, 'target_arch=="x64" and OS=="freebsd"', {
      'includes': ['config/archs/BSD-x86_64/no-asm/openssl-cl.gypi'],
    }, 'target_arch=="x64" and OS=="mac"', {
      'includes': ['config/archs/darwin64-x86_64-cc/no-asm/openssl-cl.gypi'],
    }, 'target_arch=="arm64" and OS=="mac"', {
      'includes': ['config/archs/darwin64-arm64-cc/no-asm/openssl-cl.gypi'],
    }, 'target_arch=="x64" and OS=="solaris"', {
      'includes': ['config/archs/solaris64-x86_64-gcc/no-asm/openssl-cl.gypi'],
    }, 'target_arch=="x64" and OS=="win"', {
      'includes': ['config/archs/VC-WIN64A/no-asm/openssl-cl.gypi'],
    }, 'target_arch=="x64" and OS=="linux"', {
      'includes': ['config/archs/linux-x86_64/no-asm/openssl-cl.gypi'],
    }, 'target_arch=="mips64el" and OS=="linux"', {
      'includes': ['config/archs/linux64-mips64/no-asm/openssl-cl.gypi'],
    }, 'target_arch=="riscv64" and OS=="linux"', {
      'includes': ['config/archs/linux64-riscv64/no-asm/openssl-cl.gypi'],
    }, 'target_arch=="loong64" and OS=="linux"', {
      'includes': ['config/archs/linux64-loongarch64/no-asm/openssl-cl.gypi'],
    }, {
      # Other architectures don't use assembly
      'includes': ['config/archs/linux-x86_64/no-asm/openssl-cl.gypi'],
    }],
  ],
}
