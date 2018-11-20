#include "AgoraEngine.h"

#ifdef __cplusplus
extern "C" {
#endif

static inline bool checkJNIException(JNIEnv* env) {
	bool r = false;
	if (env->ExceptionCheck()) {
		env->ExceptionDescribe();
		env->ExceptionClear();
		r = true;
	}
	return r;
}

class AgoraVideoObserver : public agora::media::IVideoFrameObserver
{
	virtual bool onCaptureVideoFrame(VideoFrame& videoFrame) {
		return true;
	}

	virtual bool onRenderVideoFrame(unsigned int uid, VideoFrame& videoFrame) {
		return true;
	}
};

class ExternalVideoRender : public agora::media::IExternalVideoRender
{
public:
	virtual void release() {
	}
	virtual int initialize() {
		return 0;
	}

	virtual int deliverFrame(const agora::media::IVideoFrame& videoFrame, int rotation, bool mirrored)
	{
		__android_log_print(ANDROID_LOG_DEBUG, TAG, "external render: w - %d, h - %d, buffer size(y) - %d",
							videoFrame.width(), videoFrame.height(),
							strlen((const char*)videoFrame.buffer(agora::media::IVideoFrame::Y_PLANE)));
		return 0;
	}
};

class ExternalVideoRenderFactory : public agora::media::IExternalVideoRenderFactory
{
	virtual agora::media::IExternalVideoRender* createRenderInstance(const agora::media::ExternalVideoRenerContext& context)
	{
		__android_log_print(ANDROID_LOG_DEBUG, TAG, "external render: createRenderInstance");
		ExternalVideoRender* r = new ExternalVideoRender();
		return r;
	}
};

static AgoraVideoObserver sVideoObserver;
static ExternalVideoRenderFactory sVideoFactory;

JavaVM* gJvm = nullptr;

AgoraEngine::AgoraEngine(JNIEnv* env, jobject context, bool async) :
				mCreateRemoteViewMethodId(nullptr)
				, mOnMessageMethodId(nullptr)
				, mAsync(async)
{
	if (mAsync) {
		mThread.reset(new std::thread(std::bind(&AgoraEngine::createEngine, this, env, context)));
	} else {
		mJavaActivity = env->NewGlobalRef(context);
		createEngine(env, context);
	}
}

AgoraEngine::~AgoraEngine()
{
	if (mThread && mThread->joinable()) {
		mThread->join();
		mThread.reset();
	}

	if (mEngine) mEngine.reset();

	if (mHandler) mHandler.reset();

	if (mJavaActivity) {
		JNIEnv* env;
		jint r = gJvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
		if (r == JNI_EDETACHED) {
			// Attach the thread to the Java VM.
			r = gJvm->AttachCurrentThread(&env, nullptr);
		}
		if (r)
			return;

		env->DeleteGlobalRef(mJavaActivity);
		gJvm->DetachCurrentThread();
	}
}

void AgoraEngine::createEngine(JNIEnv* env, jobject context)
{
	mEngine.reset(createAgoraRtcEngine());
	agora::rtc::RtcEngineContext rtcContext;
	rtcContext.appId = "aab8b8f5a8cd4469a63042fcfafe7063";
	mHandler.reset(new AgoraHandler(*this));
	rtcContext.eventHandler = mHandler.get();
	rtcContext.context = context;
	int r = mEngine->initialize(rtcContext);
	if (r) {
		__android_log_print(ANDROID_LOG_ERROR, TAG, "create Agora Engine failed (code: %d)", r);
		return;
	}
	__android_log_print(ANDROID_LOG_DEBUG, TAG, "Create Agora Engine successfully");
	agora::base::AParameter msp(*mEngine);
	if (msp) {
		msp->setParameters("{\"rtc.log_filter\": 65535}");
	}

	mEngine->enableVideo();
	mEngine->setVideoProfile(agora::rtc::VIDEO_PROFILE_PORTRAIT_360P, false);
	mEngine->setChannelProfile(agora::rtc::CHANNEL_PROFILE_LIVE_BROADCASTING);
	mEngine->setClientRole(agora::rtc::CLIENT_ROLE_BROADCASTER);

	jclass activityClass = env->GetObjectClass(mJavaActivity);
	mCreateRemoteViewMethodId = env->GetMethodID(activityClass, "createRemoteVideo", "(I)V");

	mOnMessageMethodId = env->GetMethodID(activityClass, "onMessage", "(I[B)V");

	/*
    agora::util::AutoPtr<agora::media::IMediaEngine> mediaEngine;
    mediaEngine.queryInterface(mEngine.get(), agora::AGORA_IID_MEDIA_ENGINE);
    if (mediaEngine) {
        // mediaEngine->registerVideoFrameObserver(&sVideoObserver);
        // mediaEngine->registerVideoRenderFactory(&sVideoFactory);
    }
    */
}

int AgoraEngine::setupLocalVideo(JNIEnv* env, jobject thiz, jobject v)
{
	/*
    jclass activityClass = env->GetObjectClass(thiz);
    jmethodID getViewMid = env->GetMethodID(activityClass, "getSurfaceView", "()Landroid/view/SurfaceView;");
    jobject view = env->CallObjectMethod(thiz, getViewMid);
    if (checkJNIException(env)) {
        return -agora::ERR_ABORTED;
    }
    */

	mEngine->setVideoProfile(agora::rtc::VIDEO_PROFILE_PORTRAIT_360P, false);
	agora::rtc::VideoCanvas canvas(v, agora::rtc::RENDER_MODE_HIDDEN, nullptr);
	canvas.uid = 0;
	canvas.priv = env;
	int r = mEngine->setupLocalVideo(canvas);
	if (r) {
		__android_log_print(ANDROID_LOG_ERROR, TAG, "set local video failed: %d", r);
	}
	return r;
}

int AgoraEngine::createRemoteVideo(uid_t uid)
{
	if (!gJvm)
		return -1;

	JNIEnv* env;
	jint r = gJvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
	if (r == JNI_EDETACHED) {
		// Attach the thread to the Java VM.
		r = gJvm->AttachCurrentThread(&env, nullptr);
	}
	if (r)
		return -1;

	env->CallVoidMethod(mJavaActivity, mCreateRemoteViewMethodId, uid);

	gJvm->DetachCurrentThread();
	return 0;
}

int AgoraEngine::setupRemoteVideo(JNIEnv* env, jobject thiz, uid_t uid, jobject view)
{
	/*
    jclass activityClass = env->GetObjectClass(thiz);
    jmethodID getViewMid = env->GetMethodID(activityClass, "getRemoteSurfaceView", "()Landroid/view/SurfaceView;");
    jobject view = env->CallObjectMethod(thiz, getViewMid);
    */

	agora::rtc::VideoCanvas canvas(view, agora::rtc::RENDER_MODE_HIDDEN, uid);
	return mEngine->setupRemoteVideo(canvas);
}

int AgoraEngine::joinChannel(const char* channelId)
{
	__android_log_print(ANDROID_LOG_ERROR, TAG, "join channel: %s", channelId);
	return mEngine->joinChannel(nullptr, channelId, nullptr, 0);
}

int AgoraEngine::leaveChannel()
{
	return mEngine->leaveChannel();
}

int AgoraEngine::switchCamera()
{
	return mEngine->switchCamera();
}

int AgoraEngine::muteLocalAudioStream(bool mute)
{
	return mEngine->muteLocalAudioStream(mute);
}

int AgoraEngine::muteLocalVideoStream(bool mute)
{
	return mEngine->muteLocalVideoStream(mute);
}

int AgoraEngine::onMessage(int messageId, std::string* payload)
{
	if (!mOnMessageMethodId)
		return -agora::ERR_NOT_INITIALIZED;

	JNIEnv* env;
	jint r = gJvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
	if (r == JNI_EDETACHED) {
		// Attach the thread to the Java VM.
		r = gJvm->AttachCurrentThread(&env, nullptr);
	}

	if (r || !env)
		return -agora::ERR_NOT_READY;

	jbyteArray jpayload = nullptr;
	if (payload && !payload->empty())
	{
		jpayload = env->NewByteArray(payload->length());
		if (!jpayload)
			return -agora::ERR_FAILED;
		env->SetByteArrayRegion(jpayload, 0, payload->length(), (jbyte*)payload->data());
	}

	env->CallVoidMethod(mJavaActivity, mOnMessageMethodId, (jint)messageId, jpayload);
	checkJNIException(env);
	if (jpayload)
		env->DeleteLocalRef(jpayload);

	return gJvm->DetachCurrentThread();
}

JNIEXPORT int JNICALL Java_io_agora_tutorials1v1vcall_VideoChatViewActivity_startAgoraEngine(JNIEnv * env , jobject thiz,
																							 jobject context, jboolean async)
{
	AgoraEngine* engine = new AgoraEngine(env, context, async);
	return reinterpret_cast<long>(engine);
}

JNIEXPORT int JNICALL Java_io_agora_tutorials1v1vcall_VideoChatViewActivity_setupLocalView(JNIEnv * env , jobject thiz, jlong engine, jobject v)
{
	if (!engine) return -1;

	AgoraEngine* e = reinterpret_cast<AgoraEngine*>(engine);
	return e->setupLocalVideo(env, thiz, v);
}

JNIEXPORT int JNICALL Java_io_agora_tutorials1v1vcall_VideoChatViewActivity_setupRemoteView(JNIEnv * env, jobject thiz, jlong engine, jint uid, jobject view)
{
	if (!engine) return -1;

	AgoraEngine* e = reinterpret_cast<AgoraEngine*>(engine);
	__android_log_print(ANDROID_LOG_DEBUG, TAG, "c++2 uid: %u", uid);
	return e->setupRemoteVideo(env, thiz, uid, view);
}

JNIEXPORT int JNICALL Java_io_agora_tutorials1v1vcall_VideoChatViewActivity_joinChannel(JNIEnv* env, jobject thiz, jlong engine, jstring channelId)
{
	if (!engine) return -1;

	AgoraEngine* e = reinterpret_cast<AgoraEngine*>(engine);
	const char *nativeString = env->GetStringUTFChars(channelId, JNI_FALSE);
	int r = e->joinChannel(nativeString);
	env->ReleaseStringUTFChars(channelId, nativeString);
	return r;
}

JNIEXPORT int JNICALL Java_io_agora_tutorials1v1vcall_VideoChatViewActivity_leaveChannel(JNIEnv* env, jobject thiz, jlong engine)
{
	if (!engine) return -1;

	AgoraEngine* e = reinterpret_cast<AgoraEngine*>(engine);
	return e->leaveChannel();
}

JNIEXPORT int JNICALL Java_io_agora_tutorials1v1vcall_VideoChatViewActivity_switchCamera(JNIEnv* env, jobject thiz,
																						 jlong engine)
{
	if (!engine) return -1;

	AgoraEngine* e = reinterpret_cast<AgoraEngine*>(engine);
	return e->switchCamera();
}

JNIEXPORT int JNICALL Java_io_agora_tutorials1v1vcall_VideoChatViewActivity_muteLocalAudioStream(JNIEnv* env, jobject thiz,
																								 jlong engine, jboolean mute)
{
	if (!engine) return -1;

	AgoraEngine* e = reinterpret_cast<AgoraEngine*>(engine);
	return e->muteLocalAudioStream(mute);
}

JNIEXPORT int JNICALL Java_io_agora_tutorials1v1vcall_VideoChatViewActivity_muteLocalVideoStream(JNIEnv* env, jobject thiz,
																								 jlong engine, jboolean mute)
{
	if (!engine) return -1;

	AgoraEngine* e = reinterpret_cast<AgoraEngine*>(engine);
	return e->muteLocalVideoStream(mute);
}

JNIEXPORT int JNICALL Java_io_agora_tutorials1v1vcall_VideoChatViewActivity_stopAgoraEngine(JNIEnv * env , jobject thiz, jobject context, jlong engine)
{
	AgoraEngine* e = reinterpret_cast<AgoraEngine*>(engine);
	if (!e) {
		return -1;
	}
	delete(e);
	e = nullptr;
	return 0;
}

jint JNICALL JNI_OnLoad(JavaVM* jvm, void* reserved)
{
	jint result = JNI_VERSION_1_6;
	JNIEnv *env = nullptr;

	if (jvm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6) != JNI_OK) {
		__android_log_print(ANDROID_LOG_ERROR, TAG, "JVM GetEnv failed: %s", __FUNCTION__);
		result = JNI_ERR;
		goto bail;
	}

	gJvm = jvm;

	return result;

  bail:
	return result;
}

#ifdef __cplusplus
}
#endif
