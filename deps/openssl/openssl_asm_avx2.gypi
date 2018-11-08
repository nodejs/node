{
  'conditions': [
    ['target_arch=="ppc" and OS=="aix"', {
      'includes': ['config/archs/aix-gcc/asm_avx2/openssl.gypi'],
    }, 'target_arch=="ppc" and OS=="linux"', {
      'includes': ['config/archs/linux-ppc/asm_avx2/openssl.gypi'],
    }, 'target_arch=="ppc64" and OS=="aix"', {
      'includes': ['config/archs/aix64-gcc/asm_avx2/openssl.gypi'],
    }, 'target_arch=="ppc64" and OS=="linux" and node_byteorder =="little"', {
      'includes': ['config/archs/linux-ppc64le/asm_avx2/openssl.gypi'],
    }, 'target_arch=="ppc64" and OS=="linux"', {
      'includes': ['config/archs/linux-ppc64/asm_avx2/openssl.gypi'],
    }, 'target_arch=="s390" and OS=="linux"', {
      'includes': ['config/archs/linux32-s390x/asm_avx2/openssl.gypi'],
    }, 'target_arch=="s390x" and OS=="linux"', {
      'includes': ['config/archs/linux64-s390x/asm_avx2/openssl.gypi'],
    }, 'target_arch=="arm" and OS=="linux"', {
      'includes': ['config/archs/linux-armv4/asm_avx2/openssl.gypi'],
    }, 'target_arch=="arm64" and OS=="linux"', {
      'includes': ['config/archs/linux-aarch64/asm_avx2/openssl.gypi'],
    }, 'target_arch=="ia32" and OS=="linux"', {
      'includes': ['config/archs/linux-elf/asm_avx2/openssl.gypi'],
    }, 'target_arch=="ia32" and OS=="mac"', {
      'includes': ['config/archs/darwin-i386-cc/asm_avx2/openssl.gypi'],
    }, 'target_arch=="ia32" and OS=="solaris"', {
      'includes': ['config/archs/solaris-x86-gcc/asm_avx2/openssl.gypi'],
    }, 'target_arch=="ia32" and OS=="win"', {
      'includes': ['config/archs/VC-WIN32/asm_avx2/openssl.gypi'],
      'rules': [
        {
          'rule_name': 'Assemble',
          'extension': 'asm',
          'inputs': [],
          'outputs': [
            '<(INTERMEDIATE_DIR)/<(RULE_INPUT_ROOT).obj',
          ],
          'action': [
            'nasm.exe',
            '-f win32',
            '-o', '<(INTERMEDIATE_DIR)/<(RULE_INPUT_ROOT).obj',
            '<(RULE_INPUT_PATH)',
          ],
        }
      ],
    }, 'target_arch=="ia32"', {
      'includes': ['config/archs/linux-elf/asm_avx2/openssl.gypi'],
    }, 'target_arch=="x64" and OS=="freebsd"', {
      'includes': ['config/archs/BSD-x86_64/asm_avx2/openssl.gypi'],
    }, 'target_arch=="x64" and OS=="mac"', {
      'includes': ['config/archs/darwin64-x86_64-cc/asm_avx2/openssl.gypi'],
    }, 'target_arch=="x64" and OS=="solaris"', {
      'includes': ['config/archs/solaris64-x86_64-gcc/asm_avx2/openssl.gypi'],
    }, 'target_arch=="x64" and OS=="win"', {
      'includes': ['config/archs/VC-WIN64A/asm_avx2/openssl.gypi'],
      'rules': [
        {
          'rule_name': 'Assemble',
          'extension': 'asm',
          'inputs': [],
          'outputs': [
            '<(INTERMEDIATE_DIR)/<(RULE_INPUT_ROOT).obj',
          ],
          'action': [
            'nasm.exe',
            '-f win64',
            '-DNEAR',
            '-Ox',
            '-g',
            '-o', '<(INTERMEDIATE_DIR)/<(RULE_INPUT_ROOT).obj',
            '<(RULE_INPUT_PATH)',
          ],
        }
      ],
    }, 'target_arch=="x64" and OS=="linux"', {
      'includes': ['config/archs/linux-x86_64/asm_avx2/openssl.gypi'],
    }, {
      # Other architectures don't use assembly
      'includes': ['config/archs/linux-x86_64/asm_avx2/openssl.gypi'],
    }],
  ],
}
