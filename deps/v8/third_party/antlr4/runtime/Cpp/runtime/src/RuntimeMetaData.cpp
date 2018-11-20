/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#include "RuntimeMetaData.h"

using namespace antlr4;

const std::string RuntimeMetaData::VERSION = "4.7.1";

std::string RuntimeMetaData::getRuntimeVersion() { return VERSION; }

void RuntimeMetaData::checkVersion(const std::string& generatingToolVersion,
                                   const std::string& compileTimeVersion) {
  std::string runtimeVersion = VERSION;
  bool runtimeConflictsWithGeneratingTool = false;
  bool runtimeConflictsWithCompileTimeTool = false;

  if (generatingToolVersion != "") {
    runtimeConflictsWithGeneratingTool =
        runtimeVersion != generatingToolVersion &&
        getMajorMinorVersion(runtimeVersion) !=
            getMajorMinorVersion(generatingToolVersion);
  }

  runtimeConflictsWithCompileTimeTool =
      runtimeVersion != compileTimeVersion &&
      getMajorMinorVersion(runtimeVersion) !=
          getMajorMinorVersion(compileTimeVersion);

  if (runtimeConflictsWithGeneratingTool) {
    std::cerr << "ANTLR Tool version " << generatingToolVersion
              << " used for code generation does not match "
                 "the current runtime version "
              << runtimeVersion << std::endl;
  }
  if (runtimeConflictsWithCompileTimeTool) {
    std::cerr << "ANTLR Runtime version " << compileTimeVersion
              << " used for parser compilation does not match "
                 "the current runtime version "
              << runtimeVersion << std::endl;
  }
}

std::string RuntimeMetaData::getMajorMinorVersion(const std::string& version) {
  size_t firstDot = version.find('.');
  size_t secondDot = firstDot != std::string::npos
                         ? version.find('.', firstDot + 1)
                         : std::string::npos;
  size_t firstDash = version.find('-');
  size_t referenceLength = version.size();
  if (secondDot != std::string::npos) {
    referenceLength = std::min(referenceLength, secondDot);
  }

  if (firstDash != std::string::npos) {
    referenceLength = std::min(referenceLength, firstDash);
  }

  return version.substr(0, referenceLength);
}
