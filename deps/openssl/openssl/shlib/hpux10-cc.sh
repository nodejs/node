#!/usr/bin/sh
#
# Run this script from the OpenSSL root directory:
# sh shlib/hpux10-cc.sh
# 
# HP-UX (10.20) shared library installation:
# Compile and install OpenSSL with best possible optimization:
# - shared libraries are compiled and installed with +O4 optimization
# - executable(s) are compiled and installed with +O4 optimization
# - static libraries are compiled and installed with +O3 optimization,
#   to avoid the time consuming +O4 link-time optimization when using
#   these libraries. (The shared libs are already optimized during build
#   at +O4.)
#
# This script must be run with appropriate privileges to install into
# /usr/local/ssl. HP-UX prevents used executables and shared libraries
# from being deleted or overwritten. Stop all processes using already
# installed items of OpenSSL.
#
# WARNING: At high optimization levels, HP's ANSI-C compiler can chew up
#          large amounts of memory and CPU time. Make sure to have at least
#          128MB of RAM available and that your kernel is configured to allow
#          at least 128MB data size (maxdsiz parameter which can be obtained
#          by multiplying 'echo maxdsiz/D | adb -k /stand/vmunix /dev/kmem'
#          by 'getconf PAGE_SIZE').
#          The installation process can take several hours, even on fast
#          machines. +O4 optimization of the libcrypto.sl shared library may
#          take 1 hour on a C200 (200MHz PA8200 CPU), +O3 compilation of
#          fcrypt_b.c can take 20 minutes on this machine. Stay patient.
#
# SITEFLAGS: site specific flags. I do use +DAportable, since I have to
# support older PA1.1-type CPUs. Your mileage may vary.
# +w1 enables enhanced warnings, useful when working with snaphots.
#
SITEFLAGS="+DAportable +w1"
#
# Set the default additions to build with HP-UX.
# -D_REENTRANT must/should be defined on HP-UX manually, since we do call
# Configure directly.
# +Oall increases the optimization done.
#
MYFLAGS="-D_REENTRANT +Oall $SITEFLAGS"

# Configure for pic and build the static pic libraries
perl5 Configure no-shared hpux-parisc-cc-o4 +Z ${MYFLAGS}
make clean
make DIRS="crypto ssl"
# Rename the static pic libs and build dynamic libraries from them
# Be prepared to see a lot of warnings about shared libraries being built
# with optimizations higher than +O2. When using these libraries, it is
# not possible to replace internal library functions with functions from
# the program to be linked.
#
make -f shlib/Makefile.hpux10-cc

# Copy the libraries to /usr/local/ssl/lib (they have to be in their
# final location when linking applications).
# If the directories are still there, no problem.
mkdir /usr/local
mkdir /usr/local/ssl
mkdir /usr/local/ssl/lib
chmod 444 lib*_pic.a
chmod 555 lib*.sl.0.9.8
cp -p lib*_pic.a lib*.sl.0.9.8 /usr/local/ssl/lib
(cd /usr/local/ssl/lib ; ln -sf libcrypto.sl.0.9.8 libcrypto.sl ; ln -sf libssl.sl.0.9.8 libssl.sl)

# Reconfigure without pic to compile the executables. Unfortunately, while
# performing this task we have to recompile the library components, even
# though we use the already installed shared libs anyway.
#
perl5 Configure no-shared hpux-parisc-cc-o4 ${MYFLAGS}

make clean

# Hack the Makefiles to pick up the dynamic libraries during linking
#
sed 's/^PEX_LIBS=.*$/PEX_LIBS=-L\/usr\/local\/ssl\/lib/' Makefile.ssl >xxx; mv xxx Makefile.ssl
sed 's/-L\.\.//' apps/Makefile.ssl >xxx; mv xxx apps/Makefile.ssl
sed 's/-L\.\.//' test/Makefile.ssl >xxx; mv xxx test/Makefile.ssl
# Build the static libs and the executables in one make.
make
# Install everything
make install

# Finally build the static libs with +O3. This time we only need the libraries,
# once created, they are simply copied into place.
#
perl5 Configure no-shared hpux-parisc-cc ${MYFLAGS}
make clean
make DIRS="crypto ssl"
chmod 644 libcrypto.a libssl.a
cp -p libcrypto.a libssl.a /usr/local/ssl/lib
