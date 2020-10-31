/*
 * Copyright (C) 2019 The Android Open Source Project
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

#include "src/profiling/perf/event_config.h"

#include <linux/perf_event.h>
#include <stdint.h>

#include "perfetto/base/logging.h"
#include "perfetto/ext/base/optional.h"
#include "test/gtest_and_gmock.h"

#include "protos/perfetto/config/data_source_config.gen.h"
#include "protos/perfetto/config/profiling/perf_event_config.gen.h"

namespace perfetto {
namespace profiling {
namespace {

bool IsPowerOfTwo(size_t v) {
  return (v != 0 && ((v & (v - 1)) == 0));
}

static DataSourceConfig AsDataSourceConfig(
    const protos::gen::PerfEventConfig& perf_cfg) {
  protos::gen::DataSourceConfig ds_cfg;
  ds_cfg.set_perf_event_config_raw(perf_cfg.SerializeAsString());
  return ds_cfg;
}

TEST(EventConfigTest, AttrStructConstructed) {
  protos::gen::PerfEventConfig cfg;
  base::Optional<EventConfig> event_config =
      EventConfig::Create(AsDataSourceConfig(cfg));

  ASSERT_TRUE(event_config.has_value());
  ASSERT_TRUE(event_config->perf_attr() != nullptr);
}

TEST(EventConfigTest, RingBufferPagesValidated) {
  {  // if unset, a default is used
    protos::gen::PerfEventConfig cfg;
    base::Optional<EventConfig> event_config =
        EventConfig::Create(AsDataSourceConfig(cfg));

    ASSERT_TRUE(event_config.has_value());
    ASSERT_GT(event_config->ring_buffer_pages(), 0u);
    ASSERT_TRUE(IsPowerOfTwo(event_config->ring_buffer_pages()));
  }
  {  // power of two pages accepted
    uint32_t num_pages = 128;
    protos::gen::PerfEventConfig cfg;
    cfg.set_ring_buffer_pages(num_pages);
    base::Optional<EventConfig> event_config =
        EventConfig::Create(AsDataSourceConfig(cfg));

    ASSERT_TRUE(event_config.has_value());
    ASSERT_EQ(event_config->ring_buffer_pages(), num_pages);
  }
  {  // entire config rejected if not a power of two of pages
    protos::gen::PerfEventConfig cfg;
    cfg.set_ring_buffer_pages(7);
    base::Optional<EventConfig> event_config =
        EventConfig::Create(AsDataSourceConfig(cfg));

    ASSERT_FALSE(event_config.has_value());
  }
}

TEST(EventConfigTest, ReadTickPeriodDefaultedIfUnset) {
  {  // if unset, a default is used
    protos::gen::PerfEventConfig cfg;
    base::Optional<EventConfig> event_config =
        EventConfig::Create(AsDataSourceConfig(cfg));

    ASSERT_TRUE(event_config.has_value());
    ASSERT_GT(event_config->read_tick_period_ms(), 0u);
  }
  {  // otherwise, given value used
    uint32_t period_ms = 250;
    protos::gen::PerfEventConfig cfg;
    cfg.set_ring_buffer_read_period_ms(period_ms);
    base::Optional<EventConfig> event_config =
        EventConfig::Create(AsDataSourceConfig(cfg));

    ASSERT_TRUE(event_config.has_value());
    ASSERT_EQ(event_config->read_tick_period_ms(), period_ms);
  }
}

TEST(EventConfigTest, RemotePeriodTimeoutDefaultedIfUnset) {
  {  // if unset, a default is used
    protos::gen::PerfEventConfig cfg;
    base::Optional<EventConfig> event_config =
        EventConfig::Create(AsDataSourceConfig(cfg));

    ASSERT_TRUE(event_config.has_value());
    ASSERT_GT(event_config->remote_descriptor_timeout_ms(), 0u);
  }
  {  // otherwise, given value used
    uint32_t timeout_ms = 300;
    protos::gen::PerfEventConfig cfg;
    cfg.set_remote_descriptor_timeout_ms(timeout_ms);
    base::Optional<EventConfig> event_config =
        EventConfig::Create(AsDataSourceConfig(cfg));

    ASSERT_TRUE(event_config.has_value());
    ASSERT_EQ(event_config->remote_descriptor_timeout_ms(), timeout_ms);
  }
}

}  // namespace
}  // namespace profiling
}  // namespace perfetto
