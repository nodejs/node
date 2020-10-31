/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package android.perfetto.producer;

import android.app.Activity;
import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;

public class ProducerActivity extends Activity {

    @Override
    public void onResume() {
        super.onResume();

        NotificationManager manager =
                (NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE);

        NotificationChannel serviceChannel =
                new NotificationChannel("service", "service", NotificationManager.IMPORTANCE_LOW);
        serviceChannel.setDescription("Perfetto service");
        serviceChannel.setLockscreenVisibility(Notification.VISIBILITY_PRIVATE);
        manager.createNotificationChannel(serviceChannel);

        NotificationChannel isolatedChannel = new NotificationChannel(
                "isolated_service", "isolated_service", NotificationManager.IMPORTANCE_LOW);
        isolatedChannel.setDescription("Perfetto isolated service");
        isolatedChannel.setLockscreenVisibility(Notification.VISIBILITY_PRIVATE);
        manager.createNotificationChannel(isolatedChannel);

        startForegroundService(new Intent(ProducerActivity.this, ProducerService.class));
        startForegroundService(new Intent(ProducerActivity.this, ProducerIsolatedService.class));

        System.loadLibrary("perfettocts_jni");

        // We make sure at the C++ level that we don't setup multiple producers in the same
        // process.
        new Thread(new Runnable() {
            public void run() {
                try {
                    setupProducer();
                } catch (Exception ex) {
                    ex.printStackTrace();
                }
            }
        }).start();
    }

    @Override
    public void onDestroy() {
        super.onDestroy();

        quitTaskRunner();
    }

    private static native void setupProducer();

    private static native void quitTaskRunner();
}
