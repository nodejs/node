from __future__ import print_function
import os
import re

def get_has_quic(include_path):
  if include_path:
    openssl_crypto_h = os.path.join(
        include_path,
        'openssl',
        'crypto.h')

    f = open(openssl_crypto_h)

    regex = '^#\s*define OPENSSL_INFO_QUIC'

    for line in f:
      if (re.match(regex, line)):
        return True

  return False
