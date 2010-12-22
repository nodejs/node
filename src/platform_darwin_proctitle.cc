// Copyright 2010, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// This code emanates from the Chromium project:
// http://src.chromium.org/viewvc/chrome/trunk/src/base/mac_util.mm

#include <CoreFoundation/CoreFoundation.h>
#include <Carbon/Carbon.h> // Oh noes! See discussion blow at GetCurrentProcess
#ifndef NDEBUG
  #include <err.h>
#endif

namespace node {

void Platform::SetProcessTitle(char *title) {
  static int symbol_lookup_status = 0; // 1=ok, 2=unavailable
  if (symbol_lookup_status == 2) {
    // feature is unavailable
    return;
  }

  if (process_title) free(process_title);
  process_title = strdup(title);

  // Warning: here be dragons! This is SPI reverse-engineered from WebKit's
  // plugin host, and could break at any time (although realistically it's only
  // likely to break in a new major release).
  // When 10.7 is available, check that this still works, and update this
  // comment for 10.8.

  // Private CFType used in these LaunchServices calls.
  typedef CFTypeRef PrivateLSASN;
  typedef PrivateLSASN (*LSGetCurrentApplicationASNType)();
  typedef OSStatus (*LSSetApplicationInformationItemType)(int, PrivateLSASN,
                                                          CFStringRef,
                                                          CFStringRef,
                                                          CFDictionaryRef*);

  static LSGetCurrentApplicationASNType ls_get_current_application_asn_func =
      NULL;
  static LSSetApplicationInformationItemType
      ls_set_application_information_item_func = NULL;
  static CFStringRef ls_display_name_key = NULL;
  if (!symbol_lookup_status) {
    CFBundleRef launch_services_bundle =
        CFBundleGetBundleWithIdentifier(CFSTR("com.apple.LaunchServices"));
    if (!launch_services_bundle) {
#ifndef NDEBUG
      warnx("failed to look up LaunchServices bundle");
#endif
      symbol_lookup_status = 2;
      return;
    }

    ls_get_current_application_asn_func =
        reinterpret_cast<LSGetCurrentApplicationASNType>(
            CFBundleGetFunctionPointerForName(
                launch_services_bundle, CFSTR("_LSGetCurrentApplicationASN")));
    if (!ls_get_current_application_asn_func) {
#ifndef NDEBUG
      warnx("could not find _LSGetCurrentApplicationASN");
#endif
      symbol_lookup_status = 2;
      return;
    }

    ls_set_application_information_item_func =
        reinterpret_cast<LSSetApplicationInformationItemType>(
            CFBundleGetFunctionPointerForName(
                launch_services_bundle,
                CFSTR("_LSSetApplicationInformationItem")));
    if (!ls_set_application_information_item_func) {
#ifndef NDEBUG
      warnx("Could not find _LSSetApplicationInformationItem");
#endif
      symbol_lookup_status = 2;
      return;
    }

    const CFStringRef* key_pointer = reinterpret_cast<const CFStringRef*>(
        CFBundleGetDataPointerForName(launch_services_bundle,
                                      CFSTR("_kLSDisplayNameKey")));
    ls_display_name_key = key_pointer ? *key_pointer : NULL;
    if (!ls_display_name_key) {
#ifndef NDEBUG
      warnx("Could not find _kLSDisplayNameKey");
#endif
      symbol_lookup_status = 2;
      return;
    }

    // Internally, this call relies on the Mach ports that are started up by the
    // Carbon Process Manager.  In debug builds this usually happens due to how
    // the logging layers are started up; but in release, it isn't started in as
    // much of a defined order.  So if the symbols had to be loaded, go ahead
    // and force a call to make sure the manager has been initialized and hence
    // the ports are opened.
    ProcessSerialNumber psn;
    GetCurrentProcess(&psn);
    symbol_lookup_status = 1; // 1=ok
  }

  PrivateLSASN asn = ls_get_current_application_asn_func();
  // Constant used by WebKit; what exactly it means is unknown.
  const int magic_session_constant = -2;
  CFStringRef process_name =
    CFStringCreateWithCString(NULL, title, kCFStringEncodingUTF8);
  OSErr err =
      ls_set_application_information_item_func(magic_session_constant, asn,
                                               ls_display_name_key,
                                               process_name,
                                               NULL /* optional out param */);
#ifndef NDEBUG
  if (err) {
    warnx("Call LSSetApplicationInformationItem failed");
  }
#endif
}

}  // namespace node
