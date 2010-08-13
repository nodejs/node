// Copyright 2010 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
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

#ifndef V8_V8_PROFILER_H_
#define V8_V8_PROFILER_H_

#include "v8.h"

#ifdef _WIN32
// Setup for Windows DLL export/import. See v8.h in this directory for
// information on how to build/use V8 as a DLL.
#if defined(BUILDING_V8_SHARED) && defined(USING_V8_SHARED)
#error both BUILDING_V8_SHARED and USING_V8_SHARED are set - please check the\
  build configuration to ensure that at most one of these is set
#endif

#ifdef BUILDING_V8_SHARED
#define V8EXPORT __declspec(dllexport)
#elif USING_V8_SHARED
#define V8EXPORT __declspec(dllimport)
#else
#define V8EXPORT
#endif

#else  // _WIN32

// Setup for Linux shared library export. See v8.h in this directory for
// information on how to build/use V8 as shared library.
#if defined(__GNUC__) && (__GNUC__ >= 4) && defined(V8_SHARED)
#define V8EXPORT __attribute__ ((visibility("default")))
#else  // defined(__GNUC__) && (__GNUC__ >= 4)
#define V8EXPORT
#endif  // defined(__GNUC__) && (__GNUC__ >= 4)

#endif  // _WIN32


/**
 * Profiler support for the V8 JavaScript engine.
 */
namespace v8 {


/**
 * CpuProfileNode represents a node in a call graph.
 */
class V8EXPORT CpuProfileNode {
 public:
  /** Returns function name (empty string for anonymous functions.) */
  Handle<String> GetFunctionName() const;

  /** Returns resource name for script from where the function originates. */
  Handle<String> GetScriptResourceName() const;

  /**
   * Returns the number, 1-based, of the line where the function originates.
   * kNoLineNumberInfo if no line number information is available.
   */
  int GetLineNumber() const;

  /**
   * Returns total (self + children) execution time of the function,
   * in milliseconds, estimated by samples count.
   */
  double GetTotalTime() const;

  /**
   * Returns self execution time of the function, in milliseconds,
   * estimated by samples count.
   */
  double GetSelfTime() const;

  /** Returns the count of samples where function exists. */
  double GetTotalSamplesCount() const;

  /** Returns the count of samples where function was currently executing. */
  double GetSelfSamplesCount() const;

  /** Returns function entry UID. */
  unsigned GetCallUid() const;

  /** Returns child nodes count of the node. */
  int GetChildrenCount() const;

  /** Retrieves a child node by index. */
  const CpuProfileNode* GetChild(int index) const;

  static const int kNoLineNumberInfo = Message::kNoLineNumberInfo;
};


/**
 * CpuProfile contains a CPU profile in a form of two call trees:
 *  - top-down (from main() down to functions that do all the work);
 *  - bottom-up call graph (in backward direction).
 */
class V8EXPORT CpuProfile {
 public:
  /** Returns CPU profile UID (assigned by the profiler.) */
  unsigned GetUid() const;

  /** Returns CPU profile title. */
  Handle<String> GetTitle() const;

  /** Returns the root node of the bottom up call tree. */
  const CpuProfileNode* GetBottomUpRoot() const;

  /** Returns the root node of the top down call tree. */
  const CpuProfileNode* GetTopDownRoot() const;
};


/**
 * Interface for controlling CPU profiling.
 */
class V8EXPORT CpuProfiler {
 public:
  /**
   * A note on security tokens usage. As scripts from different
   * origins can run inside a single V8 instance, it is possible to
   * have functions from different security contexts intermixed in a
   * single CPU profile. To avoid exposing function names belonging to
   * other contexts, filtering by security token is performed while
   * obtaining profiling results.
   */

  /**
   * Returns the number of profiles collected (doesn't include
   * profiles that are being collected at the moment of call.)
   */
  static int GetProfilesCount();

  /** Returns a profile by index. */
  static const CpuProfile* GetProfile(
      int index,
      Handle<Value> security_token = Handle<Value>());

  /** Returns a profile by uid. */
  static const CpuProfile* FindProfile(
      unsigned uid,
      Handle<Value> security_token = Handle<Value>());

  /**
   * Starts collecting CPU profile. Title may be an empty string. It
   * is allowed to have several profiles being collected at
   * once. Attempts to start collecting several profiles with the same
   * title are silently ignored. While collecting a profile, functions
   * from all security contexts are included in it. The token-based
   * filtering is only performed when querying for a profile.
   */
  static void StartProfiling(Handle<String> title);

  /**
   * Stops collecting CPU profile with a given title and returns it.
   * If the title given is empty, finishes the last profile started.
   */
  static const CpuProfile* StopProfiling(
      Handle<String> title,
      Handle<Value> security_token = Handle<Value>());
};


class HeapGraphNode;


/**
 * HeapSnapshotEdge represents a directed connection between heap
 * graph nodes: from retaners to retained nodes.
 */
class V8EXPORT HeapGraphEdge {
 public:
  enum Type {
    CONTEXT_VARIABLE = 0,  // A variable from a function context.
    ELEMENT = 1,           // An element of an array.
    PROPERTY = 2,          // A named object property.
    INTERNAL = 3           // A link that can't be accessed from JS,
                           // thus, its name isn't a real property name.
  };

  /** Returns edge type (see HeapGraphEdge::Type). */
  Type GetType() const;

  /**
   * Returns edge name. This can be a variable name, an element index, or
   * a property name.
   */
  Handle<Value> GetName() const;

  /** Returns origin node. */
  const HeapGraphNode* GetFromNode() const;

  /** Returns destination node. */
  const HeapGraphNode* GetToNode() const;
};


class V8EXPORT HeapGraphPath {
 public:
  /** Returns the number of edges in the path. */
  int GetEdgesCount() const;

  /** Returns an edge from the path. */
  const HeapGraphEdge* GetEdge(int index) const;

  /** Returns origin node. */
  const HeapGraphNode* GetFromNode() const;

  /** Returns destination node. */
  const HeapGraphNode* GetToNode() const;
};


/**
 * HeapGraphNode represents a node in a heap graph.
 */
class V8EXPORT HeapGraphNode {
 public:
  enum Type {
    INTERNAL = 0,   // Internal node, a virtual one, for housekeeping.
    ARRAY = 1,      // An array of elements.
    STRING = 2,     // A string.
    OBJECT = 3,     // A JS object (except for arrays and strings).
    CODE = 4,       // Compiled code.
    CLOSURE = 5     // Function closure.
  };

  /** Returns node type (see HeapGraphNode::Type). */
  Type GetType() const;

  /**
   * Returns node name. Depending on node's type this can be the name
   * of the constructor (for objects), the name of the function (for
   * closures), string value, or an empty string (for compiled code).
   */
  Handle<String> GetName() const;

  /**
   * Returns node id. For the same heap object, the id remains the same
   * across all snapshots.
   */
  uint64_t GetId() const;

  /** Returns node's own size, in bytes. */
  int GetSelfSize() const;

  /** Returns node's network (self + reachable nodes) size, in bytes. */
  int GetTotalSize() const;

  /**
   * Returns node's private size, in bytes. That is, the size of memory
   * that will be reclaimed having this node collected.
   */
  int GetPrivateSize() const;

  /** Returns child nodes count of the node. */
  int GetChildrenCount() const;

  /** Retrieves a child by index. */
  const HeapGraphEdge* GetChild(int index) const;

  /** Returns retainer nodes count of the node. */
  int GetRetainersCount() const;

  /** Returns a retainer by index. */
  const HeapGraphEdge* GetRetainer(int index) const;

  /** Returns the number of simple retaining paths from the root to the node. */
  int GetRetainingPathsCount() const;

  /** Returns a retaining path by index. */
  const HeapGraphPath* GetRetainingPath(int index) const;
};


class V8EXPORT HeapSnapshotsDiff {
 public:
  /** Returns the root node for added nodes. */
  const HeapGraphNode* GetAdditionsRoot() const;

  /** Returns the root node for deleted nodes. */
  const HeapGraphNode* GetDeletionsRoot() const;
};


/**
 * HeapSnapshots record the state of the JS heap at some moment.
 */
class V8EXPORT HeapSnapshot {
 public:
  /** Returns heap snapshot UID (assigned by the profiler.) */
  unsigned GetUid() const;

  /** Returns heap snapshot title. */
  Handle<String> GetTitle() const;

  /** Returns the root node of the heap graph. */
  const HeapGraphNode* GetRoot() const;

  /** Returns a diff between this snapshot and another one. */
  const HeapSnapshotsDiff* CompareWith(const HeapSnapshot* snapshot) const;
};


/**
 * Interface for controlling heap profiling.
 */
class V8EXPORT HeapProfiler {
 public:
  /** Returns the number of snapshots taken. */
  static int GetSnapshotsCount();

  /** Returns a snapshot by index. */
  static const HeapSnapshot* GetSnapshot(int index);

  /** Returns a profile by uid. */
  static const HeapSnapshot* FindSnapshot(unsigned uid);

  /** Takes a heap snapshot and returns it. Title may be an empty string. */
  static const HeapSnapshot* TakeSnapshot(Handle<String> title);
};


}  // namespace v8


#undef V8EXPORT


#endif  // V8_V8_PROFILER_H_
