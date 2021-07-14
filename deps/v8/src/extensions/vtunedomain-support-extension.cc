// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/extensions/vtunedomain-support-extension.h"
#include <string>
#include <vector>

namespace v8 {
namespace internal {

namespace libvtune {

int startTask(const std::vector<std::string>& vparams);
int endTask(const std::vector<std::string>& vparams);

const auto& function_map =
    *new std::map<std::string, int (*)(const std::vector<std::string>&)>{
        {"start", startTask}, {"end", endTask}};

void split(const std::string& str, char delimiter,
           std::vector<std::string>* vparams) {
  std::string::size_type baseindex = 0;
  std::string::size_type offindex = str.find(delimiter);

  while (offindex != std::string::npos) {
    (*vparams).push_back(str.substr(baseindex, offindex - baseindex));
    baseindex = ++offindex;
    offindex = str.find(delimiter, offindex);

    if (offindex == std::string::npos)
      (*vparams).push_back(str.substr(baseindex, str.length()));
  }
}

int startTask(const std::vector<std::string>& vparams) {
  int errcode = 0;

  if (const char* domain_name = vparams[1].c_str()) {
    if (const char* task_name = vparams[2].c_str()) {
      if (std::shared_ptr<VTuneDomain> domainptr =
              VTuneDomain::createDomain(domain_name)) {
        if (!domainptr->beginTask(task_name)) {
          errcode += TASK_BEGIN_FAILED;
        }
      } else {
        errcode += CREATE_DOMAIN_FAILED;
      }
    } else {
      errcode += NO_TASK_NAME;
    }

  } else {
    errcode = NO_DOMAIN_NAME;
  }

  return errcode;
}

int endTask(const std::vector<std::string>& vparams) {
  int errcode = 0;

  if (const char* domain_name = vparams[1].c_str()) {
    if (std::shared_ptr<VTuneDomain> domainptr =
            VTuneDomain::createDomain(domain_name)) {
      domainptr->endTask();
    } else {
      errcode += CREATE_DOMAIN_FAILED;
    }
  } else {
    errcode = NO_DOMAIN_NAME;
  }

  return errcode;
}

int invoke(const char* params) {
  int errcode = 0;
  std::vector<std::string> vparams;

  split(*(new std::string(params)), ' ', &vparams);

  auto it = function_map.find(vparams[0]);
  if (it != function_map.end()) {
    (it->second)(vparams);
  } else {
    errcode += UNKNOWN_PARAMS;
  }

  return errcode;
}

}  // namespace libvtune

v8::Local<v8::FunctionTemplate>
VTuneDomainSupportExtension::GetNativeFunctionTemplate(
    v8::Isolate* isolate, v8::Local<v8::String> str) {
  return v8::FunctionTemplate::New(isolate, VTuneDomainSupportExtension::Mark);
}

// args should take three parameters
// %0 : string, which is the domain name. Domain is used to tagging trace data
// for different modules or libraryies in a program
// %1 : string, which is the task name. Task is a logical unit of work performed
// by a particular thread statement. Task can nest.
// %2 : string, "start" / "end". Action to be taken on a task in a particular
// domain
void VTuneDomainSupportExtension::Mark(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  if (args.Length() != 3 || !args[0]->IsString() || !args[1]->IsString() ||
      !args[2]->IsString()) {
    args.GetIsolate()->ThrowError(
        "Parameter number should be exactly three, first domain name"
        "second task name, third start/end");
    return;
  }

  v8::Isolate* isolate = args.GetIsolate();
  v8::String::Utf8Value domainName(isolate, args[0]);
  v8::String::Utf8Value taskName(isolate, args[1]);
  v8::String::Utf8Value statName(isolate, args[2]);

  char* cdomainName = *domainName;
  char* ctaskName = *taskName;
  char* cstatName = *statName;

  std::stringstream params;
  params << cstatName << " " << cdomainName << " " << ctaskName;

  int r = 0;
  if ((r = libvtune::invoke(params.str().c_str())) != 0) {
    args.GetIsolate()->ThrowError(
        v8::String::NewFromUtf8(args.GetIsolate(), std::to_string(r).c_str())
            .ToLocalChecked());
  }
}

}  // namespace internal
}  // namespace v8
