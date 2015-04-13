#include "notification-channel.h"

#include "util.h"
#include "util-inl.h"

namespace node {

NotificationChannel::NotificationChannel(uv_loop_t* loop,
                                         NotificationCallback callback,
                                         void* data)
    : event_loop_(loop),
      callback_(callback),
      data_(data) {
  CHECK_EQ(0, uv_async_init(event_loop(),
                            notification_signal_async(),
                            AsyncCallback));
  Ref();
  notification_signal_async()->data = this;
}

void NotificationChannel::Dispose() {
  CHECK(!uv_is_closing(reinterpret_cast<uv_handle_t*>(
      notification_signal_async())));
  uv_close(reinterpret_cast<uv_handle_t*>(notification_signal_async()),
           CloseCallback);

  while (!close_callback_called_)
    uv_run(event_loop(), UV_RUN_ONCE);

  notification_signal_async()->data = nullptr;
}

void NotificationChannel::Ref() {
  uv_ref(reinterpret_cast<uv_handle_t*>(notification_signal_async()));
}

void NotificationChannel::Unref() {
  uv_unref(reinterpret_cast<uv_handle_t*>(notification_signal_async()));
}

void NotificationChannel::Notify() {
  uv_async_send(notification_signal_async());
}

void NotificationChannel::AsyncCallback(uv_async_t* handle) {
  NotificationChannel* channel =
      static_cast<NotificationChannel*>(handle->data);
  if (channel == nullptr ||
      uv_is_closing(reinterpret_cast<uv_handle_t*>(
          channel->notification_signal_async())))
    return;

  channel->callback_(channel->data_);
}

void NotificationChannel::CloseCallback(uv_handle_t* handle) {
  NotificationChannel* channel =
      static_cast<NotificationChannel*>(handle->data);
  channel->close_callback_called_ = true;
}

}  // namespace node
