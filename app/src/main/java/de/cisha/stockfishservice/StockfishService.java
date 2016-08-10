package de.cisha.stockfishservice;

import android.app.Service;
import android.content.Intent;
import android.os.IBinder;

public class StockfishService extends Service {

    static {
        System.loadLibrary("native-lib");
    }

    public StockfishService() {
    }

    @Override
    public IBinder onBind(Intent intent) {
        // TODO: Return the communication channel to the service.
        throw new UnsupportedOperationException("Not yet implemented");
    }
}
