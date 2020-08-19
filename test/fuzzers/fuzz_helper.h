struct Argv {
 public:
  Argv() : Argv({"node", "-p", "process.version"}) {}

  Argv(const std::initializer_list<const char*> &args) {
    nr_args_ = args.size();
    int total_len = 0;
    for (auto it = args.begin(); it != args.end(); ++it) {
      total_len += strlen(*it) + 1;
    }
    argv_ = static_cast<char**>(malloc(nr_args_ * sizeof(char*)));
    argv_[0] = static_cast<char*>(malloc(total_len));
    int i = 0;
    int offset = 0;
    for (auto it = args.begin(); it != args.end(); ++it, ++i) {
      int len = strlen(*it) + 1;
      snprintf(argv_[0] + offset, len, "%s", *it);
      // Skip argv_[0] as it points the correct location already
      if (i > 0) {
        argv_[i] = argv_[0] + offset;
      }
      offset += len;
    }
  }

  ~Argv() {
    free(argv_[0]);
    free(argv_);
  }

  int nr_args() const {
    return nr_args_;
  }

  char** operator*() const {
    return argv_;
  }

 private:
  char** argv_;
  int nr_args_;
};
