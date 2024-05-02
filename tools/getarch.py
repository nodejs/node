from __future__ import print_function
from utils import GuessArchitecture
arch = GuessArchitecture()

# assume 64 bit unless set specifically
print(GuessArchitecture() \
    .replace('ia32', 'x64') \
    .replace('ppc', 'ppc64') \
    .replace('arm', 'arm64') \
    .replace('s390', 's390x'))
