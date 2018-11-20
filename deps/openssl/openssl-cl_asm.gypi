{
  'conditions': [
    ['target_arch=="ppc" and OS=="aix"', {
      'includes': ['config/archs/aix-gcc/asm/openssl-cl.gypi'],
    }, 'target_arch=="ppc" and OS=="linux"', {
      'includes': ['config/archs/linux-ppc/asm/openssl-cl.gypi'],
    }, 'target_arch=="ppc64" and OS=="aix"', {
      'includes': ['config/archs/aix64-gcc/asm/openssl-cl.gypi'],
    }, 'target_arch=="ppc64" and OS=="linux" and node_byteorder =="little"', {
      'includes': ['config/archs/linux-ppc64le/asm/openssl-cl.gypi'],
    }, 'target_arch=="ppc64" and OS=="linux"', {
      'includes': ['config/archs/linux-ppc64/asm/openssl-cl.gypi'],
    }, 'target_arch=="s390" and OS=="linux"', {
      'includes': ['config/archs/linux32-s390x/asm/openssl-cl.gypi'],
    }, 'target_arch=="s390x" and OS=="linux"', {
      'includes': ['config/archs/linux64-s390x/asm/openssl-cl.gypi'],
    }, 'target_arch=="arm" and OS=="linux"', {
      'includes': ['config/archs/linux-armv4/asm/openssl-cl.gypi'],
    }, 'target_arch=="arm64" and OS=="linux"', {
      'includes': ['config/archs/linux-aarch64/asm/openssl-cl.gypi'],
    }, 'target_arch=="ia32" and OS=="linux"', {
      'includes': ['config/archs/linux-elf/asm/openssl-cl.gypi'],
    }, 'target_arch=="ia32" and OS=="mac"', {
      'includes': ['config/archs/darwin-i386-cc/asm/openssl-cl.gypi'],
    }, 'target_arch=="ia32" and OS=="solaris"', {
      'includes': ['config/archs/solaris-x86-gcc/asm/openssl-cl.gypi'],
    }, 'target_arch=="ia32" and OS=="win"', {
      'includes': ['config/archs/VC-WIN32/asm/openssl-cl.gypi'],
    }, 'target_arch=="ia32"', {
      # noasm linux-elf for other ia32 platforms
      'includes': ['config/archs/linux-elf/asm/openssl-cl.gypi'],
    }, 'target_arch=="x64" and OS=="freebsd"', {
      'includes': ['config/archs/BSD-x86_64/asm/openssl-cl.gypi'],
    }, 'target_arch=="x64" and OS=="mac"', {
      'includes': ['config/archs/darwin64-x86_64-cc/asm/openssl-cl.gypi'],
    }, 'target_arch=="x64" and OS=="solaris"', {
      'includes': ['config/archs/solaris64-x86_64-gcc/asm/openssl-cl.gypi'],
    }, 'target_arch=="x64" and OS=="win"', {
      'includes': ['config/archs/VC-WIN64A/asm/openssl-cl.gypi'],
    }, 'target_arch=="x64" and OS=="linux"', {
      'includes': ['config/archs/linux-x86_64/asm/openssl-cl.gypi'],
    }, {
      # Other architectures don't use assembly
      'includes': ['config/archs/linux-x86_64/asm/openssl-cl.gypi'],
    }],
  ],
}
