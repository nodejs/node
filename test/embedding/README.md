# C embedding API

This file is an overview for C embedding API.
It is mostly to catch all the work in progress notes.

## The API overview 

### Error handling API
- `node_embedding_on_error`

### Global platform API
- `node_embedding_set_api_version`

- `node_embedding_run_main`
- `node_embedding_create_platform`
- `node_embedding_delete_platform`

- `node_embedding_platform_set_flags`
- `node_embedding_platform_set_args`

- `node_embedding_platform_get_parsed_args`

### Runtime API
- `node_embedding_run_runtime`
- `node_embedding_create_runtime`
- `node_embedding_delete_runtime`

- `node_embedding_runtime_set_flags`
- `node_embedding_runtime_set_args`
- `node_embedding_runtime_on_preload`
- `node_embedding_runtime_on_start_execution`
- `node_embedding_runtime_add_module`
- [ ] add API to handle unhandled exceptions

### Runtime API to run event loops
- `node_embedding_on_wake_up_event_loop`
- `node_embedding_run_event_loop`
- `node_embedding_complete_event_loop`
- [ ] add API for emitting `beforeExit` event
- [ ] add API for emitting `exit` event
- [ ] add API to stop the event loop

### Runtime API to interop with Node-API
- `node_embedding_run_node_api`
- `node_embedding_open_node_api_scope`
- `node_embedding_close_node_api_scope`

### API TODOs

- [ ] Allow running Node.js uv_loop from UI loop. Follow the Electron
      implementation. - Complete implementation for non-Windows.
- [ ] Can we use some kind of waiter concept instead of the
      observer thread?
- [ ] Generate the main script based on the runtime settings.
- [ ] Set the global Inspector for he main runtime.
- [ ] Start workers from C++.
- [ ] Worker to inherit parent Inspector.
- [ ] Cancel pending event loop tasks on runtime deletion.
- [ ] Can we initialize platform again if it returns early?
- [ ] Test passing the V8 thread pool size.
- [ ] Add a way to terminate the runtime.
- [ ] Allow to provide custom thread pool from the app.
- [ ] Consider adding a v-table for the API functions to simplify
      binding with other languages.
- [ ] We must not exit the process on node::Environment errors.
- [ ] Be explicit about the recoverable errors.
- [ ] Store IsolateScope in TLS.

### Test TODOs

- [ ] Add tests based on the environment and platform `cctest`s.
- [ ] Enable the test_main_modules_node_api test.
- [ ] Test failure in Preload callback.
- [ ] Test failure in linked modules.
- [ ] Add a test that handles JS errors.
- [ ] Make sure that the delete calls match the create calls.
