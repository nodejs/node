{
  "targets": [
    {
      "target_name": "test_worker_terminate_finalization",
      "sources": [ "test_worker_terminate_finalization.c" ]
    },
    {
      "target_name": "test_worker_terminate_finalization_vtable",
      "defines": [ "NODE_API_MODULE_USE_VTABLE" ],
      "sources": [ "test_worker_terminate_finalization.c" ]
    }
  ]
}
