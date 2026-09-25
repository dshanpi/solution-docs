#ifndef T527_KVM2_PROTOCOL_H
#define T527_KVM2_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

#define KVM2_HEADER_SIZE 20u
#define KVM2_MAX_PAYLOAD 4147200u
#define KVM2_FLAG_CONFIG 1u

struct kvm2_header {
    uint32_t payload_length;
    uint64_t timestamp_us;
    uint32_t flags;
};

int kvm2_encode_header(uint8_t out[KVM2_HEADER_SIZE], const struct kvm2_header *header);
int kvm2_decode_header(const uint8_t input[KVM2_HEADER_SIZE], struct kvm2_header *header);
int kvm2_write_packet(int fd, const void *payload, size_t length, uint64_t timestamp_us, uint32_t flags);

#endif
