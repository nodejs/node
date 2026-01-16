{
  "targets": [
    {
      "target_name": "test_worker_terminate",
      "sources": [ "test_worker_terminate.c" ]
    },
    {
      "target_name": "test_worker_terminate_vtable",
      "defines": [ "NODE_API_MODULE_USE_VTABLE" ],
      "sources": [ "test_worker_terminate.c" ]
    }
  ]
}
