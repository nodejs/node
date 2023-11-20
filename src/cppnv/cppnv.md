# CPPNV

### A c++ library for processing env files adapted for nodejs


### Supports:

* Heredocs Double quoted
* Heredocs Single quoted
* Variable Interpolation (including heredocs double quoted)
* Control Characters

### Unsupported but not broken:

* Unicode (Reason being the current env parser doesn't support it either)

### Usage

```c++
  // Take a string
  string interpolate(R"(a="""
heredoc
"""
b=${a}
c=""" $ {b })");

    // turn it into a thin wrapper
  EnvStream interpolate_stream(&interpolate);
    // create a vector for them to be stored in
  std::vector<env_pair*> env_pairs;
    //read them into the vector
   EnvReader::read_pairs(&interpolate_stream, &env_pairs);

// free
 env_reader::delete_pairs(&env_pairs);

```

### See Tests for more examples
[test_dotenv.cc](..%2F..%2Ftest%2Fcctest%2Ftest_dotenv.cc)
