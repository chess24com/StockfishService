package de.cisha.stockfishservice;

import android.app.Service;
import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Message;
import android.os.Messenger;
import android.os.Parcel;
import android.os.RemoteException;

import static android.R.attr.data;

public class StockfishService extends Service {

    static {
        System.loadLibrary("stockfish-lib");
    }

    private Messenger mClient;

    private final Messenger mMessenger = new Messenger(new IncomingHandler());

    public final static String MSG_KEY = "MSG_KEY";

    class IncomingHandler extends Handler {
        @Override
        public void handleMessage(Message msg) {
            mClient = msg.replyTo;
            Bundle data = msg.peekData();
            if (data == null) {
                super.handleMessage(msg);
                return;
            }
            String line = data.getString(MSG_KEY);
            if (line == null) {
                super.handleMessage(msg);
                return;
            }
            clientToEngine(line);
        }
    }

    @Override
    public void onCreate() {
        super.onCreate();
        clientToEngine("uci\n");
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
    }

    @Override
    public IBinder onBind(Intent intent) {
        return mMessenger.getBinder();
    }

    public void engineToClient(String line) {
        if (mClient != null) {
            Bundle bundle = new Bundle();
            bundle.putString(MSG_KEY, line);
            Message msg = Message.obtain();
            msg.setData(bundle);
            try {
                mClient.send(msg);
            } catch (RemoteException e) {
                mClient = null;
            }
        }
    }

    public native void clientToEngine(String line);

}
