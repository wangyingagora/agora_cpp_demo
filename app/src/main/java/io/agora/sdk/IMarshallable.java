package io.agora.sdk;

import java.nio.ByteBuffer;

interface IMarshallable {
    public byte[] marshall();

    public void marshall(ByteBuffer buf);

    public void unmarshall(byte[] buf);

    public void unmarshall(ByteBuffer buf);

}
