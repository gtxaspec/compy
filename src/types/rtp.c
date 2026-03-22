#include <compy/types/rtp.h>

#include <assert.h>
#include <string.h>

#include <arpa/inet.h>
#include <slice99.h>

#define RTP_HEADER_VERSION_SHIFT   6
#define RTP_HEADER_PADDING_SHIFT   5
#define RTP_HEADER_EXTENSION_SHIFT 4
#define RTP_HEADER_MARKER_SHIFT    7

size_t Compy_RtpHeader_size(Compy_RtpHeader self) {
    static const size_t version_bits = 2, padding_bits = 1, extension_bits = 1,
                        csrc_count_bits = 4, marker_bits = 1,
                        payload_ty_bits = 7, sequence_number_bits = 16,
                        timestamp_bits = 32, ssrc_bits = 32;

    static const size_t csrc_size = 4;

    size_t size = (version_bits + padding_bits + extension_bits +
                   csrc_count_bits + marker_bits + payload_ty_bits +
                   sequence_number_bits + timestamp_bits + ssrc_bits) /
                      8 +
                  (self.csrc_count * csrc_size);

    if (self.extension) {
        size += sizeof(self.extension_profile) +
                sizeof(self.extension_payload_len) +
                self.extension_payload_len * sizeof(uint32_t);
    }

    return size;
}

uint8_t *
Compy_RtpHeader_serialize(Compy_RtpHeader self, uint8_t buffer[restrict]) {
    assert(buffer);

    uint8_t *buffer_backup = buffer;

    /*
     *  0                   1                   2                   3
     *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
     * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     * |V=2|P|X|  CC   |M|     PT      |       sequence number         |
     * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     * |                           timestamp                           |
     * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     * |           synchronization source (SSRC) identifier            |
     * +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
     * |            contributing source (CSRC) identifiers             |
     * |                             ....                              |
     * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     */

    // Serialize self.version, self.padding, self.extension, and self.csrc_count
    // into the first byte.
    *buffer = (self.version << RTP_HEADER_VERSION_SHIFT) | (self.csrc_count);
    if (self.padding) {
        *buffer |= ((uint8_t)1 << RTP_HEADER_PADDING_SHIFT);
    }

    if (self.extension) {
        *buffer |= ((uint8_t)1 << RTP_HEADER_EXTENSION_SHIFT);
    }

    buffer++;

    // Serialize self.marker and self.payload_ty into the second byte.
    *buffer = self.payload_ty;
    if (self.marker) {
        *buffer |= ((uint8_t)1 << RTP_HEADER_MARKER_SHIFT);
    }

    buffer++;

    buffer = SLICE99_APPEND(buffer, self.sequence_number);
    buffer = SLICE99_APPEND(buffer, self.timestamp);
    buffer = SLICE99_APPEND(buffer, self.ssrc);

    for (uint8_t i = 0; i < self.csrc_count; i++) {
        buffer = SLICE99_APPEND(buffer, self.csrc[i]);
    }

    if (self.extension) {
        buffer = SLICE99_APPEND(buffer, self.extension_profile);
        buffer = SLICE99_APPEND(buffer, self.extension_payload_len);

        for (uint16_t i = 0; i < self.extension_payload_len * sizeof(uint32_t);
             i++) {
            buffer = SLICE99_APPEND(buffer, self.extension_payload[i]);
        }
    }

    return buffer_backup;
}

int Compy_RtpHeader_deserialize(
    Compy_RtpHeader *restrict self, const uint8_t *data, size_t len) {
    assert(self);
    assert(data);

    if (len < 12) {
        return -1;
    }

    const uint8_t *p = data;

    self->version = (p[0] >> RTP_HEADER_VERSION_SHIFT) & 0x03;
    if (self->version != 2) {
        return -1;
    }

    self->padding = (p[0] >> RTP_HEADER_PADDING_SHIFT) & 0x01;
    self->extension = (p[0] >> RTP_HEADER_EXTENSION_SHIFT) & 0x01;
    self->csrc_count = p[0] & 0x0F;

    self->marker = (p[1] >> RTP_HEADER_MARKER_SHIFT) & 0x01;
    self->payload_ty = p[1] & 0x7F;

    p += 2;

    memcpy(&self->sequence_number, p, sizeof(self->sequence_number));
    p += sizeof(self->sequence_number);

    memcpy(&self->timestamp, p, sizeof(self->timestamp));
    p += sizeof(self->timestamp);

    memcpy(&self->ssrc, p, sizeof(self->ssrc));
    p += sizeof(self->ssrc);

    const size_t csrc_bytes = self->csrc_count * sizeof(uint32_t);
    if (len < (size_t)(p - data) + csrc_bytes) {
        return -1;
    }

    self->csrc = NULL;
    if (self->csrc_count > 0) {
        self->csrc = (uint32_t *)p;
    }
    p += csrc_bytes;

    self->extension_profile = 0;
    self->extension_payload_len = 0;
    self->extension_payload = NULL;

    if (self->extension) {
        if (len < (size_t)(p - data) + 4) {
            return -1;
        }

        memcpy(&self->extension_profile, p, sizeof(self->extension_profile));
        p += sizeof(self->extension_profile);

        memcpy(
            &self->extension_payload_len, p,
            sizeof(self->extension_payload_len));
        p += sizeof(self->extension_payload_len);

        const size_t ext_bytes =
            ntohs(self->extension_payload_len) * sizeof(uint32_t);
        if (len < (size_t)(p - data) + ext_bytes) {
            return -1;
        }

        self->extension_payload = (uint8_t *)p;
        p += ext_bytes;
    }

    return (int)(p - data);
}
