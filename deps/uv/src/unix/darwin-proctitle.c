/* Copyright Joyent, Inc. and other Node contributors. All rights reserved.
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <TargetConditionals.h>

#if !TARGET_OS_IPHONE
# include <CoreFoundation/CoreFoundation.h>
# include <ApplicationServices/ApplicationServices.h>
#endif


int uv__set_process_title(const char* title) {
#if TARGET_OS_IPHONE
  return -1;
#else
  typedef CFTypeRef (*LSGetCurrentApplicationASNType)(void);
  typedef OSStatus (*LSSetApplicationInformationItemType)(int,
                                                          CFTypeRef,
                                                          CFStringRef,
                                                          CFStringRef,
                                                          CFDictionaryRef*);
  typedef CFDictionaryRef (*LSApplicationCheckInType)(int, CFDictionaryRef);
  typedef OSStatus (*SetApplicationIsDaemonType)(int);
  typedef void (*LSSetApplicationLaunchServicesServerConnectionStatusType)(
      uint64_t, void*);
  CFBundleRef launch_services_bundle;
  LSGetCurrentApplicationASNType ls_get_current_application_asn;
  LSSetApplicationInformationItemType ls_set_application_information_item;
  CFStringRef* display_name_key;
  CFTypeRef asn;
  CFStringRef display_name;
  OSStatus err;
  CFBundleRef hi_services_bundle;
  LSApplicationCheckInType ls_application_check_in;
  SetApplicationIsDaemonType set_application_is_daemon;
  LSSetApplicationLaunchServicesServerConnectionStatusType
      ls_set_application_launch_services_server_connection_status;

  launch_services_bundle =
      CFBundleGetBundleWithIdentifier(CFSTR("com.apple.LaunchServices"));

  if (launch_services_bundle == NULL)
    return -1;

  ls_get_current_application_asn = (LSGetCurrentApplicationASNType)
      CFBundleGetFunctionPointerForName(launch_services_bundle,
                                        CFSTR("_LSGetCurrentApplicationASN"));

  if (ls_get_current_application_asn == NULL)
    return -1;

  ls_set_application_information_item = (LSSetApplicationInformationItemType)
      CFBundleGetFunctionPointerForName(launch_services_bundle,
                                        CFSTR("_LSSetApplicationInformationItem"));

  if (ls_set_application_information_item == NULL)
    return -1;

  display_name_key = CFBundleGetDataPointerForName(launch_services_bundle,
                                                   CFSTR("_kLSDisplayNameKey"));

  if (display_name_key == NULL || *display_name_key == NULL)
    return -1;

  /* Black 10.9 magic, to remove (Not responding) mark in Activity Monitor */
  hi_services_bundle =
      CFBundleGetBundleWithIdentifier(CFSTR("com.apple.HIServices"));
  if (hi_services_bundle == NULL)
    return -1;

  set_application_is_daemon = CFBundleGetFunctionPointerForName(
      hi_services_bundle,
      CFSTR("SetApplicationIsDaemon"));
  ls_application_check_in = CFBundleGetFunctionPointerForName(
      launch_services_bundle,
      CFSTR("_LSApplicationCheckIn"));
  ls_set_application_launch_services_server_connection_status =
      CFBundleGetFunctionPointerForName(
          launch_services_bundle,
          CFSTR("_LSSetApplicationLaunchServicesServerConnectionStatus"));
  if (set_application_is_daemon == NULL ||
      ls_application_check_in == NULL ||
      ls_set_application_launch_services_server_connection_status == NULL) {
    return -1;
  }

  if (set_application_is_daemon(1) != noErr)
    return -1;

  ls_set_application_launch_services_server_connection_status(0, NULL);

  /* Check into process manager?! */
  ls_application_check_in(-2,
                          CFBundleGetInfoDictionary(CFBundleGetMainBundle()));

  display_name = CFStringCreateWithCString(NULL, title, kCFStringEncodingUTF8);
  asn = ls_get_current_application_asn();
  err = ls_set_application_information_item(-2,  /* Magic value. */
                                            asn,
                                            *display_name_key,
                                            display_name,
                                            NULL);

  return (err == noErr) ? 0 : -1;
#endif  /* !TARGET_OS_IPHONE */
}
