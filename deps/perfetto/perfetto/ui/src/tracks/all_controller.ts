// Copyright (C) 2018 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Import all currently implemented tracks. After implemeting a new track, an
// import statement for it needs to be added here.
import './android_log/controller';
import './chrome_slices/controller';
import './counter/controller';
import './heap_profile/controller';
import './cpu_freq/controller';
import './gpu_freq/controller';
import './cpu_slices/controller';
import './process_scheduling/controller';
import './process_summary/controller';
import './thread_state/controller';
import './vsync/controller';
import './async_slices/controller';
