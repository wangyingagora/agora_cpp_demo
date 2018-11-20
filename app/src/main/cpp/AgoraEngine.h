//
// Created by eaglewangy on 2018/9/2.
//

#include <jni.h>
#include <memory>
#include <thread>
#include <string>
#include <sstream>
#include <vector>
#include <android/log.h>
#include <IAgoraRtcEngine.h>
#include <AgoraBase.h>
#include <IAgoraMediaEngine.h>

#pragma once

static const char* TAG = "AgoraEngine";

class Packer {
public:
    Packer() :
            size(2),
            position(2) {
    }

    void writeInt32(uint32_t val) {
      memcpy(&buf[0] + position, &val, sizeof(val));
      position += sizeof(val);
      size += sizeof(val);
    }

    void writeBool(bool val) {
      uint8_t v = (uint8_t)val;
      memcpy(&buf[0] + position, &v, sizeof(v));
      position += sizeof(v);
      size += sizeof(v);
    }

    std::string marshall() {
      memcpy(&buf[0], &buf[0] + 2, 2);
      return std::string(&buf[0], size);
    }
private:
    char buf[512];
    short size;
    int position;
};

class AgoraHandler;

class AgoraEngine
{
public:
    AgoraEngine(JNIEnv* env, jobject context, bool async);
    void createEngine(JNIEnv* env, jobject context);
    int setupLocalVideo(JNIEnv* env, jobject thiz, jobject view);
    int createRemoteVideo(uid_t uid);
    int setupRemoteVideo(JNIEnv* env, jobject thiz, uid_t uid, jobject view);
    int joinChannel(const char* channelId);
    int leaveChannel();
    int switchCamera();
    int muteLocalAudioStream(bool mute);
    int muteLocalVideoStream(bool mute);

    int onMessage(int messageId, std::string* payload);

    ~AgoraEngine();
private:
    bool mAsync;
    std::unique_ptr<std::thread> mThread;
    std::unique_ptr<agora::rtc::IRtcEngine> mEngine;
    std::unique_ptr<AgoraHandler> mHandler;
    jobject mJavaActivity;
    jmethodID mCreateRemoteViewMethodId;
    jmethodID mOnMessageMethodId;
};

class AgoraHandler : public agora::rtc::IRtcEngineEventHandler
{
public:
    AgoraHandler(AgoraEngine& engine) :
            mEngine(engine) {
    }

    virtual void onJoinChannelSuccess(const char* channel, uid_t uid, int elapsed) override {
      __android_log_print(ANDROID_LOG_DEBUG, TAG, "joined channel: %s, uid: %u", channel, uid);

      Packer packer;
      packer.writeInt32(uid);
      std::string payload = packer.marshall();

      mEngine.onMessage(1, &payload);
    }

    virtual void onUserJoined(uid_t uid, int elapsed) override {
      __android_log_print(ANDROID_LOG_DEBUG, TAG, "remote user joined: %u", uid);
      Packer packer;
      packer.writeInt32(uid);
      std::string payload = packer.marshall();

      mEngine.onMessage(2, &payload);
    }

    virtual void onUserOffline(uid_t uid, agora::rtc::USER_OFFLINE_REASON_TYPE reason) {
      Packer packer;
      packer.writeInt32(uid);
      packer.writeInt32((int)reason);
      std::string payload = packer.marshall();
      mEngine.onMessage(3, &payload);
    }

    virtual void onUserMuteVideo(uid_t uid, bool muted) {
      Packer packer;
      packer.writeInt32(uid);
      packer.writeBool(muted);
      std::string payload = packer.marshall();

      mEngine.onMessage(4, &payload);
    }

    virtual void onFirstRemoteVideoDecoded(uid_t uid, int width, int height, int elapsed) {
      Packer packer;
      packer.writeInt32(uid);
      std::string payload = packer.marshall();

      mEngine.onMessage(5, &payload);
    }

private:
    AgoraEngine& mEngine;
};
