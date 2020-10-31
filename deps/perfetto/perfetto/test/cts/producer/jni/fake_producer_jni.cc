/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <jni.h>

#include "perfetto/ext/traced/traced.h"
#include "perfetto/ext/tracing/ipc/default_socket.h"

#include "perfetto/ext/base/unix_task_runner.h"

#include "test/fake_producer.h"

namespace {

static std::mutex g_mutex;

// These variables are guarded by the above mutex.
static perfetto::base::UnixTaskRunner* g_activity_tr = nullptr;
static perfetto::base::UnixTaskRunner* g_service_tr = nullptr;
static perfetto::base::UnixTaskRunner* g_isolated_service_tr = nullptr;

}  // namespace

namespace perfetto {
namespace {

void ListenAndRespond(const std::string& name, base::UnixTaskRunner** tr) {
  // Note that this lock is unlocked by a post task in the middle of the
  // function instead of at the end of this function.
  std::unique_lock<std::mutex> lock(g_mutex);

  // Ensure that we don't create multiple instances of the same producer.
  // If the passed task runner is non-null, that means it's still running
  // so we don't need to create another instance.
  if (*tr)
    return;

  // Post a task to unlock the mutex when the runner has started executing
  // tasks.
  base::UnixTaskRunner task_runner;
  task_runner.PostTask([tr, &lock, &task_runner]() {
    *tr = &task_runner;
    lock.unlock();
  });

  FakeProducer producer(name, &task_runner);
  producer.Connect(
      GetProducerSocket(), [] {}, [] {}, [] {});
  task_runner.Run();

  // Cleanup the task runner again to remove outside visibilty so we can
  // create new instances of the producer.
  {
    std::lock_guard<std::mutex> guard(g_mutex);
    *tr = nullptr;
  }
}

}  // namespace
}  // namespace perfetto

extern "C" JNIEXPORT void JNICALL
Java_android_perfetto_producer_ProducerActivity_quitTaskRunner(JNIEnv*,
                                                               jclass) {
  std::lock_guard<std::mutex> guard(g_mutex);
  if (g_activity_tr) {
    g_activity_tr->Quit();
  }
}

extern "C" JNIEXPORT void JNICALL
Java_android_perfetto_producer_ProducerIsolatedService_quitTaskRunner(JNIEnv*,
                                                                      jclass) {
  std::lock_guard<std::mutex> guard(g_mutex);
  if (g_isolated_service_tr) {
    g_isolated_service_tr->Quit();
  }
}

extern "C" JNIEXPORT void JNICALL
Java_android_perfetto_producer_ProducerService_quitTaskRunner(JNIEnv*, jclass) {
  std::lock_guard<std::mutex> guard(g_mutex);
  if (g_service_tr) {
    g_service_tr->Quit();
  }
}

extern "C" JNIEXPORT void JNICALL
Java_android_perfetto_producer_ProducerActivity_setupProducer(JNIEnv*, jclass) {
  perfetto::ListenAndRespond("android.perfetto.cts.ProducerActivity",
                             &g_activity_tr);
}

extern "C" JNIEXPORT void JNICALL
Java_android_perfetto_producer_ProducerIsolatedService_setupProducer(JNIEnv*,
                                                                     jclass) {
  perfetto::ListenAndRespond("android.perfetto.cts.ProducerIsolatedService",
                             &g_isolated_service_tr);
}

extern "C" JNIEXPORT void JNICALL
Java_android_perfetto_producer_ProducerService_setupProducer(JNIEnv*, jclass) {
  perfetto::ListenAndRespond("android.perfetto.cts.ProducerService",
                             &g_service_tr);
}
