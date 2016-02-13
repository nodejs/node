/*
 * Copyright 2015 Facebook, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Significant changes made to the software:
 *
 * - Removed Boost dependency
 * - Removed support for storing values directly
 * - Removed construction and destruction of the queue items feature
 * - Added initialization of all values to nullptr
 * - Made size a template parameter
 * - Crash instead of throw if malloc fails in constructor
 * - Changed namespace from folly to node
 * - Removed sizeGuess(), isFull(), isEmpty(), popFront() and frontPtr() methods
 * - Renamed write() to PushBack(), read() to PopFront()
 * - Added padding to fields
 *
 */

// @author Bo Hu (bhu@fb.com)
// @author Jordan DeLong (delong.j@fb.com)

#ifndef SRC_PRODUCER_CONSUMER_QUEUE_H_
#define SRC_PRODUCER_CONSUMER_QUEUE_H_

#include "util.h"
#include <atomic>
#include <string.h>

namespace node {

/*
 * ProducerConsumerQueue is a one producer and one consumer queue
 * without locks.
 */
template<uint32_t size, class T>
class ProducerConsumerQueue {
  public:
    // size must be >= 2.
    //
    // Also, note that the number of usable slots in the queue at any
    // given time is actually (size-1), so if you start with an empty queue,
    // PushBack will fail after size-1 insertions.
    ProducerConsumerQueue() : readIndex_(0), writeIndex_(0) {
      STATIC_ASSERT(size >= 2);
      memset(&records_, 0, sizeof(records_[0]) * size);
    }

    ~ProducerConsumerQueue() {
      while (T* record = PopFront())
        delete record;
    }

    // Returns false if the queue is full, cannot insert nullptr
    bool PushBack(T* item) {
      CHECK_NE(item, nullptr);
      auto const currentWrite = writeIndex_.load(std::memory_order_relaxed);
      auto nextRecord = currentWrite + 1;
      if (nextRecord == size) {
        nextRecord = 0;
      }

      if (nextRecord != readIndex_.load(std::memory_order_acquire)) {
        records_[currentWrite] = item;
        writeIndex_.store(nextRecord, std::memory_order_release);
        return true;
      }

      // queue is full
      return false;
    }

    // Returns nullptr if the queue is empty.
    T* PopFront() {
      auto const currentRead = readIndex_.load(std::memory_order_relaxed);
      if (currentRead == writeIndex_.load(std::memory_order_acquire)) {
        // queue is empty
        return nullptr;
      }

      auto nextRecord = currentRead + 1;
      if (nextRecord == size) {
        nextRecord = 0;
      }
      T* ret = records_[currentRead];
      readIndex_.store(nextRecord, std::memory_order_release);
      CHECK_NE(ret, nullptr);
      return ret;
    }
  private:
    static const size_t kCacheLineLength = 128;
    typedef char padding[kCacheLineLength];
    padding padding1_;
    T* records_[size];
    padding padding2_;
    std::atomic<unsigned int> readIndex_;
    padding padding3_;
    std::atomic<unsigned int> writeIndex_;
    padding padding4_;
    DISALLOW_COPY_AND_ASSIGN(ProducerConsumerQueue);
};

}  // namespace node

#endif  // SRC_PRODUCER_CONSUMER_QUEUE_H_
