// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INSPECTOR_V8PROFILERAGENTIMPL_H_
#define V8_INSPECTOR_V8PROFILERAGENTIMPL_H_

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

  void consoleProfile(const String16& title);
  void consoleProfileEnd(const String16& title);

  bool idleStarted();
  bool idleFinished();

 private:
  String16 nextProfileId();

  void startProfiling(const String16& title);
  std::unique_ptr<protocol::Profiler::Profile> stopProfiling(
      const String16& title, bool serialize);

  bool isRecording() const;

  V8InspectorSessionImpl* m_session;
  v8::Isolate* m_isolate;
  v8::CpuProfiler* m_profiler;
  protocol::DictionaryValue* m_state;
  protocol::Profiler::Frontend m_frontend;
  bool m_enabled;
  bool m_recordingCPUProfile;
  class ProfileDescriptor;
  std::vector<ProfileDescriptor> m_startedProfiles;
  String16 m_frontendInitiatedProfileId;

  DISALLOW_COPY_AND_ASSIGN(V8ProfilerAgentImpl);
};

}  // namespace v8_inspector

#endif  // V8_INSPECTOR_V8PROFILERAGENTIMPL_H_
