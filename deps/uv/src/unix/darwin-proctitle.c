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

#include <dlfcn.h>
#include <errno.h>
#include <stdlib.h>

#include <TargetConditionals.h>

#if !TARGET_OS_IPHONE
# include <CoreFoundation/CoreFoundation.h>
# include <ApplicationServices/ApplicationServices.h>
#endif


static int uv__pthread_setname_np(const char* name) {
  int (*dynamic_pthread_setname_np)(const char* name);
  char namebuf[64];  /* MAXTHREADNAMESIZE */
  int err;

  /* pthread_setname_np() first appeared in OS X 10.6 and iOS 3.2. */
  dynamic_pthread_setname_np = dlsym(RTLD_DEFAULT, "pthread_setname_np");
  if (dynamic_pthread_setname_np == NULL)
    return -ENOSYS;

  strncpy(namebuf, name, sizeof(namebuf) - 1);
  namebuf[sizeof(namebuf) - 1] = '\0';

  err = dynamic_pthread_setname_np(namebuf);
  if (err)
    return -err;

  return 0;
}


int uv__set_process_title(const char* title) {
#if TARGET_OS_IPHONE
  return uv__pthread_setname_np(title);
#else
  typedef CFTypeRef (*LSGetCurrentApplicationASNType)(void);
  typedef OSStatus (*LSSetApplicationInformationItemType)(int,
                                                          CFTypeRef,
                                                          CFStringRef,
                                                          CFStringRef,
                                                          CFDictionaryRef*);
  CFBundleRef launch_services_bundle;
  LSGetCurrentApplicationASNType ls_get_current_application_asn;
  LSSetApplicationInformationItemType ls_set_application_information_item;
  CFStringRef* display_name_key;
  ProcessSerialNumber psn;
  CFTypeRef asn;
  CFStringRef display_name;
  OSStatus err;

  launch_services_bundle =
      CFBundleGetBundleWithIdentifier(CFSTR("com.apple.LaunchServices"));

  if (launch_services_bundle == NULL)
    return -ENOENT;

  ls_get_current_application_asn = (LSGetCurrentApplicationASNType)
      CFBundleGetFunctionPointerForName(launch_services_bundle,
                                        CFSTR("_LSGetCurrentApplicationASN"));

  if (ls_get_current_application_asn == NULL)
    return -ENOENT;

  ls_set_application_information_item = (LSSetApplicationInformationItemType)
      CFBundleGetFunctionPointerForName(launch_services_bundle,
                                        CFSTR("_LSSetApplicationInformationItem"));

  if (ls_set_application_information_item == NULL)
    return -ENOENT;

  display_name_key = CFBundleGetDataPointerForName(launch_services_bundle,
                                                   CFSTR("_kLSDisplayNameKey"));

  if (display_name_key == NULL || *display_name_key == NULL)
    return -ENOENT;

  /* Force the process manager to initialize. */
  GetCurrentProcess(&psn);

  display_name = CFStringCreateWithCString(NULL, title, kCFStringEncodingUTF8);
  asn = ls_get_current_application_asn();
  err = ls_set_application_information_item(-2,  /* Magic value. */
                                            asn,
                                            *display_name_key,
                                            display_name,
                                            NULL);
  if (err != noErr)
    return -ENOENT;

  uv__pthread_setname_np(title);  /* Don't care if it fails. */

  return 0;
#endif  /* !TARGET_OS_IPHONE */
}
