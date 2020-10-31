#include <Python.h>

#include "benchmarks.pb.h"
#include "datasets/google_message1/proto2/benchmark_message1_proto2.pb.h"
#include "datasets/google_message1/proto3/benchmark_message1_proto3.pb.h"
#include "datasets/google_message2/benchmark_message2.pb.h"
#include "datasets/google_message3/benchmark_message3.pb.h"
#include "datasets/google_message4/benchmark_message4.pb.h"

static PyMethodDef python_benchmark_methods[] = {
    {NULL, NULL, 0, NULL}        /* Sentinel */
};


PyMODINIT_FUNC
initlibbenchmark_messages() {
  benchmarks::BenchmarkDataset().descriptor();
  benchmarks::proto3::GoogleMessage1().descriptor();
  benchmarks::proto2::GoogleMessage1().descriptor();
  benchmarks::proto2::GoogleMessage2().descriptor();
  benchmarks::google_message3::GoogleMessage3().descriptor();
  benchmarks::google_message4::GoogleMessage4().descriptor();

  PyObject *m;

  m = Py_InitModule("libbenchmark_messages", python_benchmark_methods);
  if (m == NULL)
      return;
}
