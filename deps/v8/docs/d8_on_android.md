# Prerequisites
  * a Linux/Mac workstation
  * v8 r12178 (on Google Code) or later
  * an Android emulator or device with matching USB cable
  * make sure [building with GYP](http://code.google.com/p/v8-wiki/wiki/BuildingWithGYP) works


# Get the code

  * Use the instructions from https://code.google.com/p/v8-wiki/wiki/UsingGit to get the code
  * Once you need to add the android dependencies:
```
v8$ echo "target_os = ['android']" >> ../.gclient && gclient sync --nohooks
```
  * The sync will take a while the first time as it downloads the Android NDK to v8/third\_party
  * If you want to use a different NDK, you need to set the gyp variable android\_ndk\_root


# Get the Android SDK
  * tested version: `r15`
  * download the SDK from http://developer.android.com/sdk/index.html
  * extract it
  * install the "Platform tools" using the SDK manager that you can start by running `tools/android`
  * now you have a `platform_tools/adb` binary which will be used later; put it in your `PATH` or remember where it is


# Set up your device
  * Enable USB debugging (Gingerbread: Settings > Applications > Development > USB debugging; Ice Cream Sandwich: Settings > Developer Options > USB debugging)
  * connect your device to your workstation
  * make sure `adb devices` shows it; you may have to edit `udev` rules to give yourself proper permissions
  * run `adb shell` to get an ssh-like shell on the device. In that shell, do:
```
cd /data/local/tmp
mkdir v8
cd v8
```


# Push stuff onto the device
  * make sure your device is connected
  * from your workstation's shell:
```
adb push /file/you/want/to/push /data/local/tmp/v8/
```


# Compile V8 for Android
Currently two architectures (`android_arm` and `android_ia32`) are supported, each in `debug` or `release` mode. The following steps work equally well for both ARM and ia32, on either the emulator or real devices.
  * compile:
```
make android_arm.release -j16
```
  * push the resulting binary to the device:
```
adb push out/android_arm.release/d8 /data/local/tmp/v8/d8
```
  * the most comfortable way to run it is from your workstation's shell as a one-off command (rather than starting an interactive shell session on the device), that way you can use pipes or whatever to process the output as necessary:
```
adb shell /data/local/tmp/v8/d8 <parameters>
```
  * warning: when you cancel such an "adb shell whatever" command using Ctrl+C, the process on the phone will sometimes keep running.
  * Alternatively, use the `.check` suffix to automatically push test binaries and test cases onto the device and run them.
```
make android_arm.release.check
```


# Profile
  * compile a binary, push it to the device, keep a copy of it on the host
```
make android_arm.release -j16
adb push out/android_arm.release/d8 /data/local/tmp/v8/d8-version.under.test
cp out/android_arm.release/d8 ./d8-version.under.test
```
  * get a profiling log and copy it to the host:
```
adb shell /data/local/tmp/v8/d8-version.under.test benchmark.js --prof
adb pull /data/local/tmp/v8/v8.log ./
```
  * open `v8.log` in your favorite editor and edit the first line to match the full path of the `d8-version.under.test` binary on your workstation (instead of the `/data/local/tmp/v8/` path it had on the device)
  * run the tick processor with the host's `d8` and an appropriate `nm` binary:
```
cp out/ia32.release/d8 ./d8  # only required once
tools/linux-tick-processor --nm=$ANDROID_NDK_ROOT/toolchain/bin/arm-linux-androideabi-nm
```

# Compile SpiderMonkey for Lollipop
```
cd firefox/js/src
autoconf2.13
./configure \
  --target=arm-linux-androideabi \
  --with-android-ndk=$ANDROID_NDK_ROOT \
  --with-android-version=21 \
  --without-intl-api \
  --disable-tests \
  --enable-android-libstdcxx \
  --enable-pie
make
adb push -p js/src/shell/js /data/local/tmp/js
```