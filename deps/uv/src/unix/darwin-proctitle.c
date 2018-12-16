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

#include "uv.h"
#include "internal.h"

#include <dlfcn.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <TargetConditionals.h>

#if !TARGET_OS_IPHONE
# include <CoreFoundation/CoreFoundation.h>
# include <ApplicationServices/ApplicationServices.h>
#endif

#define S(s) pCFStringCreateWithCString(NULL, (s), kCFStringEncodingUTF8)


static int (*dynamic_pthread_setname_np)(const char* name);
#if !TARGET_OS_IPHONE
static CFStringRef (*pCFStringCreateWithCString)(CFAllocatorRef,
                                                 const char*,
                                                 CFStringEncoding);
static CFBundleRef (*pCFBundleGetBundleWithIdentifier)(CFStringRef);
static void *(*pCFBundleGetDataPointerForName)(CFBundleRef, CFStringRef);
static void *(*pCFBundleGetFunctionPointerForName)(CFBundleRef, CFStringRef);
static CFTypeRef (*pLSGetCurrentApplicationASN)(void);
static OSStatus (*pLSSetApplicationInformationItem)(int,
                                                    CFTypeRef,
                                                    CFStringRef,
                                                    CFStringRef,
                                                    CFDictionaryRef*);
static void* application_services_handle;
static void* core_foundation_handle;
static CFBundleRef launch_services_bundle;
static CFStringRef* display_name_key;
static CFDictionaryRef (*pCFBundleGetInfoDictionary)(CFBundleRef);
static CFBundleRef (*pCFBundleGetMainBundle)(void);
static CFBundleRef hi_services_bundle;
static OSStatus (*pSetApplicationIsDaemon)(int);
static CFDictionaryRef (*pLSApplicationCheckIn)(int, CFDictionaryRef);
static void (*pLSSetApplicationLaunchServicesServerConnectionStatus)(uint64_t,
                                                                     void*);


UV_DESTRUCTOR(static void uv__set_process_title_platform_fini(void)) {
  if (core_foundation_handle != NULL) {
    dlclose(core_foundation_handle);
    core_foundation_handle = NULL;
  }

  if (application_services_handle != NULL) {
    dlclose(application_services_handle);
    application_services_handle = NULL;
  }
}
#endif  /* !TARGET_OS_IPHONE */


void uv__set_process_title_platform_init(void) {
  /* pthread_setname_np() first appeared in OS X 10.6 and iOS 3.2. */
  *(void **)(&dynamic_pthread_setname_np) =
      dlsym(RTLD_DEFAULT, "pthread_setname_np");

#if !TARGET_OS_IPHONE
  application_services_handle = dlopen("/System/Library/Frameworks/"
                                       "ApplicationServices.framework/"
                                       "Versions/A/ApplicationServices",
                                       RTLD_LAZY | RTLD_LOCAL);
  core_foundation_handle = dlopen("/System/Library/Frameworks/"
                                  "CoreFoundation.framework/"
                                  "Versions/A/CoreFoundation",
                                  RTLD_LAZY | RTLD_LOCAL);

  if (application_services_handle == NULL || core_foundation_handle == NULL)
    goto out;

  *(void **)(&pCFStringCreateWithCString) =
      dlsym(core_foundation_handle, "CFStringCreateWithCString");
  *(void **)(&pCFBundleGetBundleWithIdentifier) =
      dlsym(core_foundation_handle, "CFBundleGetBundleWithIdentifier");
  *(void **)(&pCFBundleGetDataPointerForName) =
      dlsym(core_foundation_handle, "CFBundleGetDataPointerForName");
  *(void **)(&pCFBundleGetFunctionPointerForName) =
      dlsym(core_foundation_handle, "CFBundleGetFunctionPointerForName");

  if (pCFStringCreateWithCString == NULL ||
      pCFBundleGetBundleWithIdentifier == NULL ||
      pCFBundleGetDataPointerForName == NULL ||
      pCFBundleGetFunctionPointerForName == NULL) {
    goto out;
  }

  launch_services_bundle =
      pCFBundleGetBundleWithIdentifier(S("com.apple.LaunchServices"));

  if (launch_services_bundle == NULL)
    goto out;

  *(void **)(&pLSGetCurrentApplicationASN) =
      pCFBundleGetFunctionPointerForName(launch_services_bundle,
                                         S("_LSGetCurrentApplicationASN"));

  if (pLSGetCurrentApplicationASN == NULL)
    goto out;

  *(void **)(&pLSSetApplicationInformationItem) =
      pCFBundleGetFunctionPointerForName(launch_services_bundle,
                                         S("_LSSetApplicationInformationItem"));

  if (pLSSetApplicationInformationItem == NULL)
    goto out;

  display_name_key = pCFBundleGetDataPointerForName(launch_services_bundle,
                                                    S("_kLSDisplayNameKey"));

  if (display_name_key == NULL || *display_name_key == NULL)
    goto out;

  *(void **)(&pCFBundleGetInfoDictionary) = dlsym(core_foundation_handle,
                                     "CFBundleGetInfoDictionary");
  *(void **)(&pCFBundleGetMainBundle) = dlsym(core_foundation_handle,
                                 "CFBundleGetMainBundle");

  if (pCFBundleGetInfoDictionary == NULL || pCFBundleGetMainBundle == NULL)
    goto out;

  /* Black 10.9 magic, to remove (Not responding) mark in Activity Monitor */
  hi_services_bundle =
      pCFBundleGetBundleWithIdentifier(S("com.apple.HIServices"));

  if (hi_services_bundle == NULL)
    goto out;

  *(void **)(&pSetApplicationIsDaemon) = pCFBundleGetFunctionPointerForName(
      hi_services_bundle,
      S("SetApplicationIsDaemon"));
  *(void **)(&pLSApplicationCheckIn) = pCFBundleGetFunctionPointerForName(
      launch_services_bundle,
      S("_LSApplicationCheckIn"));
  *(void **)(&pLSSetApplicationLaunchServicesServerConnectionStatus) =
      pCFBundleGetFunctionPointerForName(
          launch_services_bundle,
          S("_LSSetApplicationLaunchServicesServerConnectionStatus"));

  if (pSetApplicationIsDaemon == NULL ||
      pLSApplicationCheckIn == NULL ||
      pLSSetApplicationLaunchServicesServerConnectionStatus == NULL) {
    goto out;
  }

  return;

out:
  uv__set_process_title_platform_fini();
#endif  /* !TARGET_OS_IPHONE */
}


void uv__set_process_title(const char* title) {
#if !TARGET_OS_IPHONE
  if (core_foundation_handle != NULL && pSetApplicationIsDaemon(1) != noErr) {
    CFTypeRef asn;
    pLSSetApplicationLaunchServicesServerConnectionStatus(0, NULL);
    pLSApplicationCheckIn(/* Magic value */ -2,
                          pCFBundleGetInfoDictionary(pCFBundleGetMainBundle()));
    asn = pLSGetCurrentApplicationASN();
    pLSSetApplicationInformationItem(/* Magic value */ -2, asn,
                                     *display_name_key, S(title), NULL);
  }
#endif  /* !TARGET_OS_IPHONE */

  if (dynamic_pthread_setname_np != NULL) {
    char namebuf[64];  /* MAXTHREADNAMESIZE */
    uv__strscpy(namebuf, title, sizeof(namebuf));
    dynamic_pthread_setname_np(namebuf);
  }
}
