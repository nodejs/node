# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'is_clang': 0,
    'gcc_version': 0,
    'openssl_no_asm%': 0
  },
  'includes': ['openssl.gypi'],
  'targets': [
    {
      'target_name': 'openssl',
      'type': '<(library)',
      'sources': [
        '<@(openssl_sources)',
      ],
      'sources/': [
        ['exclude', 'md2/.*$'],
        ['exclude', 'store/.*$']
      ],
      'conditions': [
        ['target_arch!="ia32" and target_arch!="x64" and target_arch!="arm" or openssl_no_asm!=0', {
          # Disable asm
          'defines': [
            'OPENSSL_NO_ASM'
           ],
          'sources': [
            '<@(openssl_sources_no_asm)',
          ],
        }, {
          # Enable asm
          'defines': [
            'AES_ASM',
            'CPUID_ASM',
            'OPENSSL_BN_ASM_MONT',
            'OPENSSL_CPUID_OBJ',
            'SHA1_ASM',
            'SHA256_ASM',
            'SHA512_ASM',
            'GHASH_ASM',
          ],
          'conditions': [
            # Extended assembly on non-arm platforms
            ['target_arch!="arm"', {
              'defines': [
                'VPAES_ASM',
                'BN_ASM',
                'BF_ASM',
                'BNCO_ASM',
                'DES_ASM',
                'LIB_BN_ASM',
                'MD5_ASM',
                'OPENSSL_BN_ASM',
                'RIP_ASM',
                'RMD160_ASM',
                'WHIRLPOOL_ASM',
                'WP_ASM',
              ],
            }],
            ['OS!="win" and OS!="mac" and target_arch=="ia32"', {
              'sources': [
                '<@(openssl_sources_x86_elf_gas)',
              ]
            }],
            ['OS!="win" and OS!="mac" and target_arch=="x64"', {
              'defines': [
                'OPENSSL_BN_ASM_MONT5',
                'OPENSSL_BN_ASM_GF2m',
                'OPENSSL_IA32_SSE2',
                'BSAES_ASM',
              ],
              'sources': [
                '<@(openssl_sources_x64_elf_gas)',
              ]
            }],
            ['OS=="mac" and target_arch=="ia32"', {
              'sources': [
                '<@(openssl_sources_x86_macosx_gas)',
              ]
            }],
            ['OS=="mac" and target_arch=="x64"', {
              'defines': [
                'OPENSSL_BN_ASM_MONT5',
                'OPENSSL_BN_ASM_GF2m',
                'OPENSSL_IA32_SSE2',
                'BSAES_ASM',
              ],
              'sources': [
                '<@(openssl_sources_x64_macosx_gas)',
              ]
            }],
            ['target_arch=="arm"', {
              'sources': [
                '<@(openssl_sources_arm_elf_gas)',
              ]
            }],
            ['OS=="win" and target_arch=="ia32"', {
              'sources': [
                '<@(openssl_sources_x86_win32_masm)',
              ],
              'rules': [
                {
                  'rule_name': 'Assemble',
                  'extension': 'asm',
                  'inputs': [],
                  'outputs': [
                    '<(INTERMEDIATE_DIR)/<(RULE_INPUT_ROOT).obj',
                  ],
                  'action': [
                    'ml.exe',
                    '/Zi',
                    '/safeseh',
                    '/Fo', '<(INTERMEDIATE_DIR)/<(RULE_INPUT_ROOT).obj',
                    '/c', '<(RULE_INPUT_PATH)',
                  ],
                  'process_outputs_as_sources': 0,
                  'message': 'Assembling <(RULE_INPUT_PATH) to <(INTERMEDIATE_DIR)/<(RULE_INPUT_ROOT).obj.',
                }
              ]
            }],
            ['OS=="win" and target_arch=="x64"', {
              'defines': [
                'OPENSSL_BN_ASM_MONT5',
                'OPENSSL_BN_ASM_GF2m',
                'OPENSSL_IA32_SSE2',
                'BSAES_ASM',
              ],
              'sources': [
                '<@(openssl_sources_x64_win32_masm)',
              ],
              'rules': [
                {
                  'rule_name': 'Assemble',
                  'extension': 'asm',
                  'inputs': [],
                  'outputs': [
                    '<(INTERMEDIATE_DIR)/<(RULE_INPUT_ROOT).obj',
                  ],
                  'action': [
                    'ml64.exe',
                    '/Zi',
                    '/Fo', '<(INTERMEDIATE_DIR)/<(RULE_INPUT_ROOT).obj',
                    '/c', '<(RULE_INPUT_PATH)',
                  ],
                  'process_outputs_as_sources': 0,
                  'message': 'Assembling <(RULE_INPUT_PATH) to <(INTERMEDIATE_DIR)/<(RULE_INPUT_ROOT).obj.',
                }
              ]
            }]
          ]
        }],
        ['OS=="win"', {
          'link_settings': {
            'libraries': [
              '-lgdi32.lib',
              '-luser32.lib',
            ]
          },
          'defines': [
            'DSO_WIN32',
          ],
        }, {
          'defines': [
            'DSO_DLFCN',
            'HAVE_DLFCN_H'
          ],
        }],
      ],
      'include_dirs': [
        '.',
        'openssl',
        'openssl/crypto',
        'openssl/crypto/asn1',
        'openssl/crypto/evp',
        'openssl/crypto/md2',
        'openssl/crypto/modes',
        'openssl/crypto/store',
        'openssl/include',
      ],
      'direct_dependent_settings': {
        'include_dirs': ['openssl/include'],
      },
    },
    {
      'target_name': 'openssl-cli',
      'type': 'executable',
      'dependencies': [
        'openssl',
      ],
      'defines': [
        'MONOLITH',
      ],
      'sources': [
        '<@(openssl_cli_sources)',
      ],
      'conditions': [
        ['OS=="solaris"', {
          'libraries': [
            '-lsocket',
            '-lnsl',
          ]
        }],
        ['OS=="win"', {
          'link_settings': {
            'libraries': [
              '-lws2_32.lib',
              '-lgdi32.lib',
              '-ladvapi32.lib',
              '-lcrypt32.lib',
              '-luser32.lib',
            ],
          },
        }],
        [ 'OS in "linux android"', {
          'link_settings': {
            'libraries': [
              '-ldl',
            ],
          },
        }],
      ]
    }
  ],
  'target_defaults': {
    'include_dirs': [
      '.',
      'openssl',
      'openssl/crypto',
      'openssl/crypto/asn1',
      'openssl/crypto/evp',
      'openssl/crypto/md2',
      'openssl/crypto/modes',
      'openssl/crypto/store',
      'openssl/include',
    ],
    'defines': [
      # No clue what these are for.
      'L_ENDIAN',
      'PURIFY',
      '_REENTRANT',

      # SSLv2 is known broken and has been superseded by SSLv3 for almost
      # twenty years now.
      'OPENSSL_NO_SSL2',

      # SSLv3 is susceptible to downgrade attacks (POODLE.)
      'OPENSSL_NO_SSL3',

      # Heartbeat is a TLS extension, that couldn't be turned off or
      # asked to be not advertised. Unfortunately this is unacceptable for
      # Microsoft's IIS, which seems to be ignoring whole ClientHello after
      # seeing this extension.
      'OPENSSL_NO_HEARTBEATS',
    ],
    'conditions': [
      ['OS=="win"', {
        'defines': [
          'MK1MF_BUILD',
          'WIN32_LEAN_AND_MEAN',
          'OPENSSL_SYSNAME_WIN32',
        ],
        'msvs_disabled_warnings': [
          4244, # conversion from 'signed type', possible loss of data
          4267, # conversion from 'unsigned type', possible loss of data
          4996, # 'GetVersionExA': was declared deprecated
        ],
      }, {
        'defines': [
          # ENGINESDIR must be defined if OPENSSLDIR is.
          'ENGINESDIR="/dev/null"',
          'TERMIOS',
        ],
        'cflags': ['-Wno-missing-field-initializers'],
        'conditions': [
          ['OS=="mac"', {
            'defines': [
              # Set to ubuntu default path for convenience. If necessary,
              # override this at runtime with the SSL_CERT_DIR environment
              # variable.
              'OPENSSLDIR="/System/Library/OpenSSL/"',
            ],
          }, {
            'defines': [
              # Set to ubuntu default path for convenience. If necessary,
              # override this at runtime with the SSL_CERT_DIR environment
              # variable.
              'OPENSSLDIR="/etc/ssl"',
            ],
          }],
        ]
      }],
      ['is_clang==1 or gcc_version>=43', {
        'cflags': ['-Wno-old-style-declaration'],
      }],
      ['OS=="solaris"', {
        'defines': ['__EXTENSIONS__'],
      }],
    ],
  },
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
