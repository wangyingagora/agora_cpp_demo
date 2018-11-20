package io.agora.tutorials1v1vcall;

import android.Manifest;
import android.content.Context;
import android.content.pm.PackageManager;
import android.graphics.PorterDuff;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.v4.app.ActivityCompat;
import android.support.v4.content.ContextCompat;
import android.support.v7.app.AppCompatActivity;
import android.util.Log;
import android.view.SurfaceView;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.Toast;

import io.agora.rtc.RtcEngine;
import io.agora.sdk.EnvelopeMessage;

public class VideoChatViewActivity extends AppCompatActivity {
    static {
        System.loadLibrary("agora-rtc-sdk-jni");
        System.loadLibrary("agora-engine");
    }

    private static final String LOG_TAG = VideoChatViewActivity.class.getSimpleName();

    private static final int PERMISSION_REQ_ID_RECORD_AUDIO = 22;
    private static final int PERMISSION_REQ_ID_CAMERA = PERMISSION_REQ_ID_RECORD_AUDIO + 1;

    private long mEngine;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_video_chat_view);

        if (checkSelfPermission(Manifest.permission.RECORD_AUDIO, PERMISSION_REQ_ID_RECORD_AUDIO) && checkSelfPermission(Manifest.permission.CAMERA, PERMISSION_REQ_ID_CAMERA)) {
            initAgoraEngineAndJoinChannel();
        }
    }

    private void initAgoraEngineAndJoinChannel() {
        initializeAgoraEngine();     // Tutorial Step 1
        // setupVideoProfile();         // Tutorial Step 2
        setupLocalVideo();           // Tutorial Step 3
        joinChannel();               // Tutorial Step 4
    }

    public boolean checkSelfPermission(String permission, int requestCode) {
        Log.i(LOG_TAG, "checkSelfPermission " + permission + " " + requestCode);
        if (ContextCompat.checkSelfPermission(this,
                permission)
                != PackageManager.PERMISSION_GRANTED) {

            ActivityCompat.requestPermissions(this,
                    new String[]{permission},
                    requestCode);
            return false;
        }
        return true;
    }

    @Override
    public void onRequestPermissionsResult(int requestCode,
                                           @NonNull String permissions[], @NonNull int[] grantResults) {
        Log.i(LOG_TAG, "onRequestPermissionsResult " + grantResults[0] + " " + requestCode);

        switch (requestCode) {
            case PERMISSION_REQ_ID_RECORD_AUDIO: {
                if (grantResults.length > 0
                        && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                    checkSelfPermission(Manifest.permission.CAMERA, PERMISSION_REQ_ID_CAMERA);
                } else {
                    showLongToast("No permission for " + Manifest.permission.RECORD_AUDIO);
                    finish();
                }
                break;
            }
            case PERMISSION_REQ_ID_CAMERA: {
                if (grantResults.length > 0
                        && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                    initAgoraEngineAndJoinChannel();
                } else {
                    showLongToast("No permission for " + Manifest.permission.CAMERA);
                    finish();
                }
                break;
            }
        }
    }

    public final void showLongToast(final String msg) {
        this.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                Toast.makeText(getApplicationContext(), msg, Toast.LENGTH_LONG).show();
            }
        });
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();

        leaveChannel();
        stopAgoraEngine(this, mEngine);
        mEngine = 0;
    }

    // Tutorial Step 10
    public void onLocalVideoMuteClicked(View view) {
        ImageView iv = (ImageView) view;
        if (iv.isSelected()) {
            iv.setSelected(false);
            iv.clearColorFilter();
        } else {
            iv.setSelected(true);
            iv.setColorFilter(getResources().getColor(R.color.colorPrimary), PorterDuff.Mode.MULTIPLY);
        }

        muteLocalVideoStream(mEngine, iv.isSelected());

        FrameLayout container = (FrameLayout) findViewById(R.id.local_video_view_container);
        SurfaceView surfaceView = (SurfaceView) container.getChildAt(0);
        surfaceView.setZOrderMediaOverlay(!iv.isSelected());
        surfaceView.setVisibility(iv.isSelected() ? View.GONE : View.VISIBLE);
    }

    // Tutorial Step 9
    public void onLocalAudioMuteClicked(View view) {
        ImageView iv = (ImageView) view;
        if (iv.isSelected()) {
            iv.setSelected(false);
            iv.clearColorFilter();
        } else {
            iv.setSelected(true);
            iv.setColorFilter(getResources().getColor(R.color.colorPrimary), PorterDuff.Mode.MULTIPLY);
        }

        muteLocalAudioStream(mEngine, iv.isSelected());
    }

    // Tutorial Step 8
    public void onSwitchCameraClicked(View view) {
        switchCamera(mEngine);
    }

    // Tutorial Step 6
    public void onEncCallClicked(View view) {
        finish();
    }

    public SurfaceView getSurfaceView() {
        FrameLayout layout = (FrameLayout) findViewById(R.id.local_video_view_container);
        return (SurfaceView) layout.getChildAt(0);
    }

    public SurfaceView getRemoteSurfaceView() {
        FrameLayout layout = (FrameLayout) findViewById(R.id.remote_video_view_container);
        SurfaceView surfaceView = (SurfaceView) layout.getChildAt(0);
        return surfaceView;
    }

    private void createRemoteVideo(final int uid) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                FrameLayout container = (FrameLayout) findViewById(R.id.remote_video_view_container);

                if (container.getChildCount() >= 1) {
                    return;
                }

                SurfaceView surfaceView = RtcEngine.CreateRendererView(getBaseContext());
                container.addView(surfaceView);

                surfaceView.setTag(uid); // for mark purpose
                View tipMsg = findViewById(R.id.quick_tips_when_use_agora_sdk); // optional UI
                tipMsg.setVisibility(View.GONE);

                setupRemoteView(mEngine, uid, surfaceView);
            }
        });
    }

    // Tutorial Step 1
    private void initializeAgoraEngine() {
        mEngine = startAgoraEngine(this, false);
    }

    // Tutorial Step 3
    private void setupLocalVideo() {
        FrameLayout container = (FrameLayout) findViewById(R.id.local_video_view_container);
        SurfaceView surfaceView = RtcEngine.CreateRendererView(getBaseContext());
        surfaceView.setZOrderMediaOverlay(true);
        container.addView(surfaceView);
        setupLocalView(mEngine, surfaceView);
    }

    // Tutorial Step 4
    private void joinChannel() {
        joinChannel(mEngine, "aaaa");
    }

    // Tutorial Step 6
    private void leaveChannel() {
        // mRtcEngine.leaveChannel();
        leaveChannel(mEngine);
    }

    // Tutorial Step 7
    private void onRemoteUserLeft() {
        FrameLayout container = (FrameLayout) findViewById(R.id.remote_video_view_container);
        container.removeAllViews();

        View tipMsg = findViewById(R.id.quick_tips_when_use_agora_sdk); // optional UI
        tipMsg.setVisibility(View.VISIBLE);
    }

    // Tutorial Step 10
    private void onRemoteUserVideoMuted(int uid, boolean muted) {
        FrameLayout container = (FrameLayout) findViewById(R.id.remote_video_view_container);

        SurfaceView surfaceView = (SurfaceView) container.getChildAt(0);

        if (surfaceView == null) return;

        Object tag = surfaceView.getTag();
        if (tag != null && (Integer) tag == uid) {
            surfaceView.setVisibility(muted ? View.GONE : View.VISIBLE);
        }
    }

    public void onMessage(int eventId, byte[] evt) {
        switch (eventId) {
            case 1:
                EnvelopeMessage.JoinSuccess m = new EnvelopeMessage.JoinSuccess();
                m.unmarshall(evt);
                int uid = m.uid;
                Log.d(LOG_TAG, "joined channel: " + (uid & 0xFFFFFFFFL));
                break;
            case 2:
                break;
            case 3:
                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        onRemoteUserLeft();
                    }
                });
                break;
            case 4:
                final EnvelopeMessage.UserMuteVideo mv = new EnvelopeMessage.UserMuteVideo();
                mv.unmarshall(evt);
                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        onRemoteUserVideoMuted(mv.uid, mv.mute);
                    }
                });
                break;
            case 5:
                EnvelopeMessage.CreateRemoteVideo rv = new EnvelopeMessage.CreateRemoteVideo();
                rv.unmarshall(evt);
                Log.d(LOG_TAG, "c++3 uid: " + rv.uid);
                createRemoteVideo(rv.uid);
                break;
            default:
                break;
        }
    }

    public native int startAgoraEngine(Context context, boolean async);

    public native int joinChannel(long engine, String channelId);

    public native int leaveChannel(long engine);

    public native int stopAgoraEngine(Context context, long engine);

    public native int setupLocalView(long engine, SurfaceView v);

    public native int notifyReady(long engine);

    public native int setupRemoteView(long engine, int uid, SurfaceView view);

    public native int switchCamera(long engine);

    public native int muteLocalAudioStream(long engine, boolean mute);

    public native int muteLocalVideoStream(long engine, boolean mute);
}
