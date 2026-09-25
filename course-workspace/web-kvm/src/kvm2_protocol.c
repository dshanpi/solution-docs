#include "kvm2_protocol.h"

#include <errno.h>
#include <string.h>
#include <unistd.h>

static void put32(uint8_t *p, uint32_t v) {
    p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24;
}
static uint32_t get32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static void put64(uint8_t *p, uint64_t v) { put32(p, v); put32(p + 4, v >> 32); }
static uint64_t get64(const uint8_t *p) { return get32(p) | ((uint64_t)get32(p + 4) << 32); }

int kvm2_encode_header(uint8_t out[KVM2_HEADER_SIZE], const struct kvm2_header *h) {
    if (!out || !h || h->payload_length == 0 || h->payload_length > KVM2_MAX_PAYLOAD || (h->flags & ~KVM2_FLAG_CONFIG)) return -1;
    memcpy(out, "KVM2", 4); put32(out + 4, h->payload_length); put64(out + 8, h->timestamp_us); put32(out + 16, h->flags); return 0;
}
int kvm2_decode_header(const uint8_t in[KVM2_HEADER_SIZE], struct kvm2_header *h) {
    if (!in || !h || memcmp(in, "KVM2", 4)) return -1;
    h->payload_length = get32(in + 4); h->timestamp_us = get64(in + 8); h->flags = get32(in + 16);
    return (h->payload_length && h->payload_length <= KVM2_MAX_PAYLOAD && !(h->flags & ~KVM2_FLAG_CONFIG)) ? 0 : -1;
}
static int write_all(int fd, const uint8_t *data, size_t length) {
    while (length) { ssize_t n = write(fd, data, length); if (n > 0) { data += n; length -= (size_t)n; continue; } if (n < 0 && errno == EINTR) continue; return -1; } return 0;
}
int kvm2_write_packet(int fd, const void *payload, size_t length, uint64_t timestamp_us, uint32_t flags) {
    uint8_t header[KVM2_HEADER_SIZE]; struct kvm2_header h = {(uint32_t)length, timestamp_us, flags};
    if (!payload || kvm2_encode_header(header, &h)) return -1;
    return write_all(fd, header, sizeof(header)) || write_all(fd, payload, length) ? -1 : 0;
}
