package io.agora.sdk;

public class EnvelopeMessage {
    public static class JoinSuccess extends Marshallable {
        public int uid;

        public byte[] marshall() {
            return super.marshall();
        }

        public void unmarshall(byte[] buf) {
            super.unmarshall(buf);
            uid = popInt();
        }
    }

    public static class UserOffline extends Marshallable {
        int uid;
        int reason;

        public byte[] marshall() {
            return super.marshall();
        }

        public void unmarshall(byte[] buf) {
            super.unmarshall(buf);
            uid = popInt();
            reason = popInt();
        }
    }

    public static class UserMuteVideo extends Marshallable {
        public int uid;
        public boolean mute;

        public byte[] marshall() {
            return super.marshall();
        }

        public void unmarshall(byte[] buf) {
            super.unmarshall(buf);
            uid = popInt();
            mute = popBool();
        }
    }

    public static class CreateRemoteVideo extends Marshallable {
        public int uid;

        public byte[] marshall() {
            return super.marshall();
        }

        public void unmarshall(byte[] buf) {
            super.unmarshall(buf);
            uid = popInt();
        }
    }
}
