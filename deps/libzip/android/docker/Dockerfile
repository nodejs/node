# Version: 1.0

# Dockerfile for building libzip for android
# https://github.com/dec1/libzip.git
# creates docker container with all tools, libraries and sources required to build libzip for android.

# Author: Declan Moran
# www.silverglint.com


# Usage:
#---------
# download the libzip repository
# > git clone https://github.com/dec1/libzip.git
# > cd libzip
#
# build docker image "my_img_zip" from the dockerfile in "docker" dir
# > docker build -t my_img_zip ./android/docker
#
# run docker container "my_ctr_zip" from this image, mounting the current dir. (Need to pass absolute host paths to mount volume- hence "pwd")
# > docker run  -v $(pwd):/home/docker-share/libzip -it --entrypoint=/bin/bash --name my_ctr_zip my_img_zip
#
# Now inside docker container
# $ cd /home/docker-share/libzip/android
#
# Modify ./do.sh (on host), to match the boost and android ndk versions/paths in the "Configure here" section below
# Build from running docker container.
# $./do.sh
#
# "./build" dir contains required build, but owned by root. chown to your username/group
# > sudo chown -R <userid>:<groupid> ./build
# > sudo chown -R <userid>:<groupid> ./install
#
# Exit container, when build is finished.
# $ exit
#




FROM ubuntu:18.04


## --------------------------------------------------------------------
##              Configure here
# ---------------------------------------------------------------------
# ---------------------------------------------------------------------
# Here you can speciofy exactly what android ndk (and sdk) version you want to use.



# (2) Android SDK
# https://developer.android.com/studio#downloads
ARG SDK_URL_BASE=https://dl.google.com/android/repository
ARG SDK_FILE=sdk-tools-linux-4333796.zip

# the sdk platform to use
# https://developer.android.com/guide/topics/manifest/uses-sdk-element
ARG ANDROID_SDK_PLATFORM_VERS="platforms;android-28"



# (3) Android NDK
# https://developer.android.com/ndk/downloads
ARG NDK_URL_BASE=https://dl.google.com/android/repository
ARG NDK_FILE=android-ndk-r19c-linux-x86_64.zip
# ---------------------------------------------------------------------
## --------------------------------------------------------------------

RUN apt-get update
RUN apt-get -y dist-upgrade


# for downloading archives
RUN apt-get -y install wget

# for unzipping downloaded android archives
RUN apt-get -y install zip
RUN apt-get -y install cmake

RUN apt-get -y install lib32z1


# need this this to install some (32 bit) prerequisites for android builds
RUN dpkg --add-architecture i386
RUN apt-get update
RUN apt-get -y dist-upgrade
RUN apt-get install -y  libc6:i386 libncurses5:i386 libstdc++6:i386 libbz2-1.0:i386


# need c compiler to set up create boost build system (before building boost with it and android toolchain)
RUN apt-get -y install build-essential
RUN apt-get -y install libc6-dev-i386
RUN apt-get -y install clang

RUN apt-get -y install openjdk-8-jdk
#--------------------------------------

ARG ANDROID_HOME=/home/android
WORKDIR ${ANDROID_HOME}


# SDK
# ----
# download android sdk command line tools
RUN wget ${SDK_URL_BASE}/$SDK_FILE
RUN unzip $SDK_FILE

ENV PATH ${PATH}:${ANDROID_HOME}/tools:${ANDROID_HOME}/tools/bin:${ANDROID_HOME}/platform-tools


RUN yes | sdkmanager --licenses

RUN sdkmanager "platform-tools" $ANDROID_SDK_PLATFORM_VERS
#RUN sdkmanager "platform-tools" "platforms;android-28"


# NDK
# ----
RUN wget ${NDK_URL_BASE}/$NDK_FILE
RUN unzip $NDK_FILE

