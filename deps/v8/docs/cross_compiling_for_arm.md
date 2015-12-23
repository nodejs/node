<font color='darkred'><b><h2>Building V8 with SCons is no longer supported. See <a href='https://code.google.com/p/v8-wiki/wiki/BuildingWithGYP'>BuildingWithGYP</a>.</h2></b></font>

---


# Using Sourcery G++ Lite

The Sourcery G++ Lite cross compiler suite is a free version of Sourcery G++ from [CodeSourcery](http://www.codesourcery.com). There is a page for the [GNU Toolchain for ARM Processors](http://www.codesourcery.com/sgpp/lite/arm). Determine the version you need for your host/target combination.

The following instructions uses [2009q1-203 for ARM GNU/Linux](http://www.codesourcery.com/sgpp/lite/arm/portal/release858), and if using a different version please change the URLs and `TOOL_PREFIX` below accordingly.

## Installing on host and target

The simplest way of setting this up is to install the full Sourcery G++ Lite package on both the host and target at the same location. This will ensure that all the libraries required are available on both sides. If you want to use the default libraries on the host there is no need the install anything on the target.

The following script will install in `/opt/codesourcery`:

```
#!/bin/sh

sudo mkdir /opt/codesourcery
cd /opt/codesourcery
sudo chown $USERNAME .
chmod g+ws .
umask 2
wget http://www.codesourcery.com/sgpp/lite/arm/portal/package4571/public/arm-none-linux-gnueabi/arm-2009q1-203-arm-none-linux-gnueabi-i686-pc-linux-gnu.tar.bz2
tar -xvf arm-2009q1-203-arm-none-linux-gnueabi-i686-pc-linux-gnu.tar.bz2
```


## Building using scons without snapshot

The simplest way to build is without snapshot, as that does no involve using the simulator to generate the snapshot. The following script will build the sample shell without snapshot for ARM v7.

```
#!/bin/sh

export TOOL_PREFIX=/opt/codesourcery/arm-2009q1/bin/arm-none-linux-gnueabi
export CXX=$TOOL_PREFIX-g++
export AR=$TOOL_PREFIX-ar
export RANLIB=$TOOL_PREFIX-ranlib
export CC=$TOOL_PREFIX-gcc
export LD=$TOOL_PREFIX-ld

export CCFLAGS="-march=armv7-a -mtune=cortex-a8 -mfpu=vfp"
export ARM_TARGET_LIB=/opt/codesourcery/arm-2009q1/arm-none-linux-gnueabi/libc

scons wordsize=32 snapshot=off arch=arm sample=shell
```

If the processor is not Cortex A8 or does not have VFP enabled the `-mtune=cortex-a8` and `-mfpu=vfp` part of `CCFLAGS` needs to be changed accordingly. By default the V8 SCons build adds `-mfloat-abi=softfp`.

If using the default libraries on the target just leave out the setting of `ARM_TARGET_LIB` and if the target libraies are in a different location ARM\_TARGET\_LIB` needs to be adjusted accordingly.

The default for Sourcery G++ Lite is ARM v5te with software floating point emulation, so if testing building for ARM v5te the setting of `CCFLAGS` and `ARM_TARGET_LIB` should be changed to:

```
CCFLAGS=""
ARM_TARGET_LIB=/opt/codesourcery/arm-2009q1/arm-none-linux-gnueabi/libc

scons armeabi=soft ...
```

Relying on defaults in the tool chain might lead to surprises, so for ARM v5te with software floating point emulation the following is more explicit:

```
CCFLAGS="-march=armv5te"
ARM_TARGET_LIB=/opt/codesourcery/arm-2009q1/arm-none-linux-gnueabi/libc

scons armeabi=soft ...
```

If the target has an VFP unit use the following:

```
CCFLAGS="-mfpu=vfpv3"
ARM_TARGET_LIB=/opt/codesourcery/arm-2009q1/arm-none-linux-gnueabi/libc
```

To allow G++ to use Thumb2 instructions and the VFP unit when compiling the C/C++ code use:

```
CCFLAGS="-mthumb -mfpu=vfpv3"
ARM_TARGET_LIB=/opt/codesourcery/arm-2009q1/arm-none-linux-gnueabi/libc/thumb2
```

_Note:_ V8 will not use Thumb2 instructions in its generated code it always uses the full ARM instruction set.

For other ARM versions please check the Sourcery G++ Lite documentation.

As mentioned above the default for Sourcery G++ Lite used here is ARM v5te with software floating point emulation. However beware that this default might change between versions and that there is no unique defaults for ARM tool chains in general, so always passing `-march` and possibly `-mfpu` is recommended. Passing `-mfloat-abi` is not required as this is controlled by the SCons option `armeabi`.

## Building using scons with snapshot

When building with snapshot the simulator is used to build the snapshot on the host and then building for the target with that snapshot. The following script will accomplish that (using both Thumb2 and VFP instructions):

```
#!/bin/sh

V8DIR=..

cd host

scons -Y$V8DIR simulator=arm snapshot=on
mv obj/release/snapshot.cc $V8DIR/src/snapshot.cc

cd ..

export TOOL_PREFIX=/opt/codesourcery/arm-2010.09-103/bin/arm-none-linux-gnueabi
export CXX=$TOOL_PREFIX-g++
export AR=$TOOL_PREFIX-ar
export RANLIB=$TOOL_PREFIX-ranlib
export CC=$TOOL_PREFIX-gcc
export LD=$TOOL_PREFIX-ld

export CCFLAGS="-mthumb -march=armv7-a -mfpu=vfpv3"
export ARM_TARGET_LIB=/opt/codesourcery/arm-2010.09-103/arm-none-linux-gnueabi/libc/thumb2

cd target

scons -Y$V8DIR wordsize=32 snapshot=nobuild arch=armsample=shell 
rm $V8DIR/src/snapshot.cc

cd ..
```

This script required the two subdirectories `host` and `target`. V8 is first build for the host with the ARM simulator which supports running ARM code on the host. This is used to build a snapshot file which is then used for the actual cross compilation of V8.

## Building for target which supports unaligned access

The default when building V8 for an ARM target (either cross compiling or compiling on an ARM machine) is to disable unaligned memory access. However in some situations (most noticeably handling of regular expressions) performance will be better if unaligned memory access is used on processors which supports it. To enable unaligned memory access set `unalignedaccesses` to `on` when building:

```
scons unalignedaccesses=on ...
```

When running in the simulator the default is to enable unaligned memory access, so to test in the simulator with unaligned memory access disabled set `unalignedaccesses` to `off` when building:

```
scons unalignedaccesses=off simulator=arm ...
```

## Using V8 with hardfp calling convention

By default V8 uses the softfp calling convention when calling C functions from generated code. However it is possible to use hardfp as well. To enable this set `armeabi` to `hardfp` when building:

```
scons armeabi=hardfp ...
```

Passing `armeabi=hardfp` to SCons will automatically set the compiler flag `-mfloat-abi=hardfp`. If using snapshots remember to pass `armeabi=hardfp` when building V8 on the host for generating the snapshot as well.