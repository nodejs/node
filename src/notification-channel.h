#ifndef SRC_NOTIFICATION_CHANNEL_H_
#define SRC_NOTIFICATION_CHANNEL_H_

#include "uv.h"

namespace node {

typedef void (*NotificationCallback)(void* data);

class NotificationChannel {
  public:
    NotificationChannel(uv_loop_t* event_loop,
                        NotificationCallback callback,
                        void* data);
    void Dispose();
    void Ref();
    void Unref();
    void Notify();

  private:
    uv_loop_t* event_loop() const {
      return event_loop_;
    }

    uv_async_t* notification_signal_async()  {
      return &notification_signal_async_;
    }

    uv_mutex_t* access_mutex() {
      return &access_mutex_;
    }

    static void AsyncCallback(uv_async_t* handle);
    static void CloseCallback(uv_handle_t* handle);

    uv_loop_t* const event_loop_;
    uv_async_t notification_signal_async_;
    uv_mutex_t access_mutex_;
    NotificationCallback const callback_;
    bool close_callback_called_ = false;
    void* data_;
};

}  // namespace node

#endif  // SRC_NOTIFICATION_CHANNEL_H_
