from __future__ import print_function
import os
import re

def get_has_quic(include_path):
  if include_path:
    openssl_quic_h = os.path.join(
        include_path,
        'openssl',
        'quic.h')

    try:
      f = open(openssl_quic_h)
    except OSError:
      return False

    regex = r'^#\s*define OPENSSL_INFO_QUIC'

    for line in f:
      if (re.match(regex, line)):
        return True

  return False
