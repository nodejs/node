// Copyright(c) 2015-present, Gabi Melman & spdlog contributors.
// Distributed under the MIT License (http://opensource.org/licenses/MIT)

#pragma once

#include <spdlog/common.h>
#include <spdlog/details/circular_q.h>
#include <spdlog/details/file_helper.h>
#include <spdlog/details/null_mutex.h>
#include <spdlog/details/os.h>
#include <spdlog/details/synchronous_factory.h>
#include <spdlog/fmt/fmt.h>
#include <spdlog/sinks/base_sink.h>

#include <chrono>
#include <cstdio>
#include <ctime>
#include <mutex>
#include <string>

namespace spdlog {
namespace sinks {

/*
 * Generator of Hourly log file names in format basename.YYYY-MM-DD-HH.ext
 */
struct hourly_filename_calculator {
    // Create filename for the form basename.YYYY-MM-DD-H
    static filename_t calc_filename(const filename_t &filename, const tm &now_tm) {
        filename_t basename, ext;
        std::tie(basename, ext) = details::file_helper::split_by_extension(filename);
        return fmt_lib::format(SPDLOG_FILENAME_T("{}_{:04d}-{:02d}-{:02d}_{:02d}{}"), basename,
                               now_tm.tm_year + 1900, now_tm.tm_mon + 1, now_tm.tm_mday,
                               now_tm.tm_hour, ext);
    }
};

/*
 * Rotating file sink based on time.
 * If truncate != false , the created file will be truncated.
 * If max_files > 0, retain only the last max_files and delete previous.
 * Note that old log files from previous executions will not be deleted by this class,
 * rotation and deletion is only applied while the program is running.
 */
template <typename Mutex, typename FileNameCalc = hourly_filename_calculator>
class hourly_file_sink final : public base_sink<Mutex> {
public:
    // create hourly file sink which rotates on given time
    hourly_file_sink(filename_t base_filename,
                     bool truncate = false,
                     uint16_t max_files = 0,
                     const file_event_handlers &event_handlers = {})
        : base_filename_(std::move(base_filename)),
          file_helper_{event_handlers},
          truncate_(truncate),
          max_files_(max_files),
          filenames_q_() {
        auto now = log_clock::now();
        auto filename = FileNameCalc::calc_filename(base_filename_, now_tm(now));
        file_helper_.open(filename, truncate_);
        remove_init_file_ = file_helper_.size() == 0;
        rotation_tp_ = next_rotation_tp_();

        if (max_files_ > 0) {
            init_filenames_q_();
        }
    }

    filename_t filename() {
        std::lock_guard<Mutex> lock(base_sink<Mutex>::mutex_);
        return file_helper_.filename();
    }

protected:
    void sink_it_(const details::log_msg &msg) override {
        auto time = msg.time;
        bool should_rotate = time >= rotation_tp_;
        if (should_rotate) {
            if (remove_init_file_) {
                file_helper_.close();
                details::os::remove(file_helper_.filename());
            }
            auto filename = FileNameCalc::calc_filename(base_filename_, now_tm(time));
            file_helper_.open(filename, truncate_);
            rotation_tp_ = next_rotation_tp_();
        }
        remove_init_file_ = false;
        memory_buf_t formatted;
        base_sink<Mutex>::formatter_->format(msg, formatted);
        file_helper_.write(formatted);

        // Do the cleaning only at the end because it might throw on failure.
        if (should_rotate && max_files_ > 0) {
            delete_old_();
        }
    }

    void flush_() override { file_helper_.flush(); }

private:
    void init_filenames_q_() {
        using details::os::path_exists;

        filenames_q_ = details::circular_q<filename_t>(static_cast<size_t>(max_files_));
        std::vector<filename_t> filenames;
        auto now = log_clock::now();
        while (filenames.size() < max_files_) {
            auto filename = FileNameCalc::calc_filename(base_filename_, now_tm(now));
            if (!path_exists(filename)) {
                break;
            }
            filenames.emplace_back(filename);
            now -= std::chrono::hours(1);
        }
        for (auto iter = filenames.rbegin(); iter != filenames.rend(); ++iter) {
            filenames_q_.push_back(std::move(*iter));
        }
    }

    tm now_tm(log_clock::time_point tp) {
        time_t tnow = log_clock::to_time_t(tp);
        return spdlog::details::os::localtime(tnow);
    }

    log_clock::time_point next_rotation_tp_() {
        auto now = log_clock::now();
        tm date = now_tm(now);
        date.tm_min = 0;
        date.tm_sec = 0;
        auto rotation_time = log_clock::from_time_t(std::mktime(&date));
        if (rotation_time > now) {
            return rotation_time;
        }
        return {rotation_time + std::chrono::hours(1)};
    }

    // Delete the file N rotations ago.
    // Throw spdlog_ex on failure to delete the old file.
    void delete_old_() {
        using details::os::filename_to_str;
        using details::os::remove_if_exists;

        filename_t current_file = file_helper_.filename();
        if (filenames_q_.full()) {
            auto old_filename = std::move(filenames_q_.front());
            filenames_q_.pop_front();
            bool ok = remove_if_exists(old_filename) == 0;
            if (!ok) {
                filenames_q_.push_back(std::move(current_file));
                SPDLOG_THROW(spdlog_ex(
                    "Failed removing hourly file " + filename_to_str(old_filename), errno));
            }
        }
        filenames_q_.push_back(std::move(current_file));
    }

    filename_t base_filename_;
    log_clock::time_point rotation_tp_;
    details::file_helper file_helper_;
    bool truncate_;
    uint16_t max_files_;
    details::circular_q<filename_t> filenames_q_;
    bool remove_init_file_;
};

using hourly_file_sink_mt = hourly_file_sink<std::mutex>;
using hourly_file_sink_st = hourly_file_sink<details::null_mutex>;

}  // namespace sinks

//
// factory functions
//
template <typename Factory = spdlog::synchronous_factory>
inline std::shared_ptr<logger> hourly_logger_mt(const std::string &logger_name,
                                                const filename_t &filename,
                                                bool truncate = false,
                                                uint16_t max_files = 0,
                                                const file_event_handlers &event_handlers = {}) {
    return Factory::template create<sinks::hourly_file_sink_mt>(logger_name, filename, truncate,
                                                                max_files, event_handlers);
}

template <typename Factory = spdlog::synchronous_factory>
inline std::shared_ptr<logger> hourly_logger_st(const std::string &logger_name,
                                                const filename_t &filename,
                                                bool truncate = false,
                                                uint16_t max_files = 0,
                                                const file_event_handlers &event_handlers = {}) {
    return Factory::template create<sinks::hourly_file_sink_st>(logger_name, filename, truncate,
                                                                max_files, event_handlers);
}
}  // namespace spdlog
