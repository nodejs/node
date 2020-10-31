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
import './android_log/frontend';
import './chrome_slices/frontend';
import './counter/frontend';
import './heap_profile/frontend';
import './cpu_freq/frontend';
import './gpu_freq/frontend';
import './cpu_slices/frontend';
import './process_scheduling/frontend';
import './process_summary/frontend';
import './thread_state/frontend';
import './vsync/frontend';
import './async_slices/frontend';
