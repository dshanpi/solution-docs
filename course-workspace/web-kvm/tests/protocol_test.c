#include "kvm2_protocol.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    const uint8_t expected[20] = {'K','V','M','2',3,0,0,0,0x88,0x77,0x66,0x55,0x44,0x33,0x22,0x11,1,0,0,0};
    uint8_t bytes[20]; struct kvm2_header source={3,0x1122334455667788ULL,1}, decoded={0};
    assert(kvm2_encode_header(bytes,&source)==0); assert(memcmp(bytes,expected,20)==0);
    assert(kvm2_decode_header(bytes,&decoded)==0); assert(decoded.payload_length==3 && decoded.timestamp_us==source.timestamp_us && decoded.flags==1);
    bytes[0]='X'; assert(kvm2_decode_header(bytes,&decoded)!=0); puts("KVM2_PROTOCOL_OK"); return 0;
}
