// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INSPECTOR_V8_PROFILER_AGENT_IMPL_H_
#define V8_INSPECTOR_V8_PROFILER_AGENT_IMPL_H_

#include <vector>

#include "src/base/macros.h"
#include "src/inspector/protocol/Forward.h"
#include "src/inspector/protocol/Profiler.h"

namespace v8 {
class CpuProfiler;
class Isolate;
}

namespace v8_inspector {

class V8InspectorSessionImpl;

using protocol::Maybe;
using protocol::Response;

class V8ProfilerAgentImpl : public protocol::Profiler::Backend {
 public:
  V8ProfilerAgentImpl(V8InspectorSessionImpl*, protocol::FrontendChannel*,
                      protocol::DictionaryValue* state);
  ~V8ProfilerAgentImpl() override;

  bool enabled() const { return m_enabled; }
  void restore();

  Response enable() override;
  Response disable() override;
  Response setSamplingInterval(int) override;
  Response start() override;
  Response stop(std::unique_ptr<protocol::Profiler::Profile>*) override;

  Response startPreciseCoverage(Maybe<bool> binary,
                                Maybe<bool> detailed) override;
  Response stopPreciseCoverage() override;
  Response takePreciseCoverage(
      std::unique_ptr<protocol::Array<protocol::Profiler::ScriptCoverage>>*
          out_result) override;
  Response getBestEffortCoverage(
      std::unique_ptr<protocol::Array<protocol::Profiler::ScriptCoverage>>*
          out_result) override;

  Response startTypeProfile() override;
  Response stopTypeProfile() override;
  Response takeTypeProfile(
      std::unique_ptr<protocol::Array<protocol::Profiler::ScriptTypeProfile>>*
          out_result) override;

  void consoleProfile(const String16& title);
  void consoleProfileEnd(const String16& title);

 private:
  String16 nextProfileId();

  void startProfiling(const String16& title);
  std::unique_ptr<protocol::Profiler::Profile> stopProfiling(
      const String16& title, bool serialize);

  V8InspectorSessionImpl* m_session;
  v8::Isolate* m_isolate;
  v8::CpuProfiler* m_profiler = nullptr;
  protocol::DictionaryValue* m_state;
  protocol::Profiler::Frontend m_frontend;
  bool m_enabled = false;
  bool m_recordingCPUProfile = false;
  class ProfileDescriptor;
  std::vector<ProfileDescriptor> m_startedProfiles;
  String16 m_frontendInitiatedProfileId;
  int m_startedProfilesCount = 0;

  DISALLOW_COPY_AND_ASSIGN(V8ProfilerAgentImpl);
};

}  // namespace v8_inspector

#endif  // V8_INSPECTOR_V8_PROFILER_AGENT_IMPL_H_
