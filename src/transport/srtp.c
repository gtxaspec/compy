#include <compy/transport.h>

#include <compy/priv/base64.h>
#include <compy/priv/crypto.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>

/*
 * SRTP implementation per RFC 3711.
 *
 * Cipher: AES-128-CM (counter mode)
 * Auth:   HMAC-SHA1-80 (10-byte tag) or HMAC-SHA1-32 (4-byte tag)
 */

#define SRTP_MAX_PACKET_SIZE 2048
#define SRTP_AUTH_TAG_80     10
#define SRTP_AUTH_TAG_32     4
#define AES_BLOCK_SIZE       16

/* Key derivation labels (RFC 3711 Section 4.3.1) */
#define LABEL_SRTP_ENCRYPTION  0x00
#define LABEL_SRTP_AUTH        0x01
#define LABEL_SRTP_SALT        0x02
#define LABEL_SRTCP_ENCRYPTION 0x03
#define LABEL_SRTCP_AUTH       0x04
#define LABEL_SRTCP_SALT       0x05

typedef struct {
    Compy_Transport inner;
    Compy_SrtpSuite suite;
    uint8_t session_key[16];
    uint8_t session_salt[14];
    uint8_t auth_key[20];
    uint32_t roc;      /* rollover counter */
    uint16_t prev_seq; /* previous sequence number for ROC tracking */
    int auth_tag_len;
} Compy_SrtpTransport;

declImpl(Compy_Transport, Compy_SrtpTransport);

/*
 * SRTCP transport (RFC 3711 Section 3.4).
 *
 * Differs from SRTP:
 * - Uses SRTCP key derivation labels (0x03/0x04/0x05)
 * - Appends a 4-byte SRTCP index (E-flag | 31-bit index) after payload
 * - Auth tag covers header + encrypted payload + SRTCP index
 * - Index is a monotonic counter, not derived from packet seq
 */
typedef struct {
    Compy_Transport inner;
    Compy_SrtpSuite suite;
    uint8_t session_key[16];
    uint8_t session_salt[14];
    uint8_t auth_key[20];
    uint32_t srtcp_index; /* 31-bit counter */
    int auth_tag_len;
} Compy_SrtcpTransport;

declImpl(Compy_Transport, Compy_SrtcpTransport);

/* --- Key derivation (RFC 3711 Section 4.3.1) --- */

static void srtp_kdf(
    const uint8_t master_key[16], const uint8_t master_salt[14], uint8_t label,
    uint8_t *out, size_t out_len) {
    /*
     * x = label || r (r=0 for index 0)
     * key_id = label * 2^48
     * x = key_id XOR master_salt (padded to 14 bytes)
     *
     * For key_derivation_rate = 0 (default): r = 0, so:
     * x[0..5] = master_salt[0..5]
     * x[6] = master_salt[6] ^ label
     * x[7..13] = master_salt[7..13]
     *
     * Then AES-CM with master_key to generate output.
     */
    uint8_t x[AES_BLOCK_SIZE];
    memset(x, 0, sizeof x);
    memcpy(x, master_salt, 14);
    x[7] ^= label;

    size_t generated = 0;
    uint16_t counter = 0;

    while (generated < out_len) {
        uint8_t iv[AES_BLOCK_SIZE];
        memcpy(iv, x, AES_BLOCK_SIZE);
        iv[14] = (uint8_t)(counter >> 8);
        iv[15] = (uint8_t)(counter & 0xFF);

        uint8_t keystream[AES_BLOCK_SIZE];
        compy_crypto_srtp_ops.aes128_ecb(master_key, iv, keystream);

        size_t to_copy = out_len - generated;
        if (to_copy > AES_BLOCK_SIZE) {
            to_copy = AES_BLOCK_SIZE;
        }
        memcpy(out + generated, keystream, to_copy);
        generated += to_copy;
        counter++;
    }
}

static void
srtp_derive_keys(Compy_SrtpTransport *self, const Compy_SrtpKeyMaterial *key) {
    srtp_kdf(
        key->master_key, key->master_salt, LABEL_SRTP_ENCRYPTION,
        self->session_key, 16);
    srtp_kdf(
        key->master_key, key->master_salt, LABEL_SRTP_SALT, self->session_salt,
        14);
    srtp_kdf(
        key->master_key, key->master_salt, LABEL_SRTP_AUTH, self->auth_key, 20);
}

/* --- AES-128-CM encryption (RFC 3711 Section 4.1) --- */

static void srtp_encrypt_payload(
    const uint8_t session_key[16], const uint8_t session_salt[14],
    uint32_t ssrc, uint32_t index, uint8_t *payload, size_t payload_len) {
    /*
     * IV = (session_salt padded to 16 bytes) XOR
     *      (0x0000 || SSRC || packet_index)
     *
     * session_salt is 14 bytes, left-padded with 2 zero bytes to make 16.
     * SSRC is at bytes 4-7, packet_index at bytes 8-13 of the XOR mask.
     */
    uint8_t iv[AES_BLOCK_SIZE];
    memset(iv, 0, AES_BLOCK_SIZE);
    /* Salt occupies bytes 2-15 (14 bytes) */
    for (int i = 0; i < 14; i++) {
        iv[i + 2] = session_salt[i];
    }

    /* XOR with SSRC at bytes 4-7 */
    uint32_t ssrc_be = htonl(ssrc);
    for (int i = 0; i < 4; i++) {
        iv[4 + i] ^= ((uint8_t *)&ssrc_be)[i];
    }

    /* XOR with packet index at bytes 8-13 (48-bit) */
    iv[8] ^= (uint8_t)((index >> 24) & 0xFF);
    iv[9] ^= (uint8_t)((index >> 16) & 0xFF);
    iv[10] ^= (uint8_t)((index >> 8) & 0xFF);
    iv[11] ^= (uint8_t)(index & 0xFF);

    /* Generate keystream and XOR with payload */
    size_t offset = 0;
    uint16_t counter = 0;

    while (offset < payload_len) {
        uint8_t block_iv[AES_BLOCK_SIZE];
        memcpy(block_iv, iv, AES_BLOCK_SIZE);
        block_iv[14] = (uint8_t)(counter >> 8);
        block_iv[15] = (uint8_t)(counter & 0xFF);

        uint8_t keystream[AES_BLOCK_SIZE];
        compy_crypto_srtp_ops.aes128_ecb(session_key, block_iv, keystream);

        size_t to_xor = payload_len - offset;
        if (to_xor > AES_BLOCK_SIZE) {
            to_xor = AES_BLOCK_SIZE;
        }

        for (size_t i = 0; i < to_xor; i++) {
            payload[offset + i] ^= keystream[i];
        }

        offset += to_xor;
        counter++;
    }
}

/* --- Transport interface --- */

Compy_Transport compy_transport_srtp(
    Compy_Transport inner, Compy_SrtpSuite suite,
    const Compy_SrtpKeyMaterial *key) {
    assert(inner.self && inner.vptr);
    assert(key);

    Compy_SrtpTransport *self = malloc(sizeof *self);
    assert(self);

    self->inner = inner;
    self->suite = suite;
    self->roc = 0;
    self->prev_seq = 0;

    switch (suite) {
    case Compy_SrtpSuite_AES_CM_128_HMAC_SHA1_80:
        self->auth_tag_len = SRTP_AUTH_TAG_80;
        break;
    case Compy_SrtpSuite_AES_CM_128_HMAC_SHA1_32:
        self->auth_tag_len = SRTP_AUTH_TAG_32;
        break;
    }

    srtp_derive_keys(self, key);

    return DYN(Compy_SrtpTransport, Compy_Transport, self);
}

static void Compy_SrtpTransport_drop(VSelf) {
    VSELF(Compy_SrtpTransport);
    assert(self);

    VCALL_SUPER(self->inner, Compy_Droppable, drop);
    /* Wipe keys from memory (volatile to prevent optimizer removal) */
    volatile uint8_t *p;
    p = self->session_key;
    for (size_t i = 0; i < sizeof self->session_key; i++)
        p[i] = 0;
    p = self->session_salt;
    for (size_t i = 0; i < sizeof self->session_salt; i++)
        p[i] = 0;
    p = self->auth_key;
    for (size_t i = 0; i < sizeof self->auth_key; i++)
        p[i] = 0;
    free(self);
}

impl(Compy_Droppable, Compy_SrtpTransport);

static int Compy_SrtpTransport_transmit(VSelf, Compy_IoVecSlice bufs) {
    VSELF(Compy_SrtpTransport);
    assert(self);

    /* Coalesce all iovecs into a contiguous buffer */
    size_t total = Compy_IoVecSlice_len(bufs);
    if (total + (size_t)self->auth_tag_len > SRTP_MAX_PACKET_SIZE) {
        return -1;
    }

    uint8_t packet[SRTP_MAX_PACKET_SIZE];
    size_t offset = 0;
    for (size_t i = 0; i < bufs.len; i++) {
        memcpy(packet + offset, bufs.ptr[i].iov_base, bufs.ptr[i].iov_len);
        offset += bufs.ptr[i].iov_len;
    }

    /* RTP header is at least 12 bytes */
    if (total < 12) {
        return -1;
    }

    /* Extract SSRC (bytes 8-11) and sequence number (bytes 2-3) */
    uint32_t ssrc;
    uint16_t seq;
    memcpy(&ssrc, packet + 8, 4);
    memcpy(&seq, packet + 2, 2);
    ssrc = ntohl(ssrc);
    seq = ntohs(seq);

    /* Detect sequence number rollover and increment ROC (RFC 3711 §3.3.1) */
    if (seq < self->prev_seq && (self->prev_seq - seq) > 0x7FFF) {
        self->roc++;
    }
    self->prev_seq = seq;

    /* Packet index = ROC * 65536 + SEQ */
    uint32_t index = self->roc * 65536 + seq;

    /* Determine header length (12 + CSRC count * 4) */
    uint8_t cc = packet[0] & 0x0F;
    size_t header_len = 12 + (size_t)cc * 4;
    if (header_len > total) {
        return -1;
    }

    /* Encrypt payload in-place (everything after the RTP header) */
    srtp_encrypt_payload(
        self->session_key, self->session_salt, ssrc, index, packet + header_len,
        total - header_len);

    /* Compute authentication tag over: RTP header + encrypted payload + ROC */
    uint8_t auth_input[SRTP_MAX_PACKET_SIZE + 4];
    memcpy(auth_input, packet, total);
    uint32_t roc_be = htonl(self->roc);
    memcpy(auth_input + total, &roc_be, 4);

    uint8_t hmac_out[20];
    compy_crypto_srtp_ops.hmac_sha1(
        self->auth_key, sizeof self->auth_key, auth_input, total + 4, hmac_out);

    /* Append truncated auth tag */
    memcpy(packet + total, hmac_out, (size_t)self->auth_tag_len);
    total += (size_t)self->auth_tag_len;

    /* Send encrypted packet as single iovec */
    const Compy_IoVecSlice encrypted =
        (Compy_IoVecSlice)Slice99_typed_from_array((struct iovec[]){
            {.iov_base = packet, .iov_len = total},
        });

    return VCALL(self->inner, transmit, encrypted);
}

static bool Compy_SrtpTransport_is_full(VSelf) {
    VSELF(Compy_SrtpTransport);
    return VCALL(self->inner, is_full);
}

impl(Compy_Transport, Compy_SrtpTransport);

/* --- SRTCP transport implementation (RFC 3711 Section 3.4) --- */

static void srtcp_derive_keys(
    Compy_SrtcpTransport *self, const Compy_SrtpKeyMaterial *key) {
    srtp_kdf(
        key->master_key, key->master_salt, LABEL_SRTCP_ENCRYPTION,
        self->session_key, 16);
    srtp_kdf(
        key->master_key, key->master_salt, LABEL_SRTCP_SALT, self->session_salt,
        14);
    srtp_kdf(
        key->master_key, key->master_salt, LABEL_SRTCP_AUTH, self->auth_key,
        20);
}

Compy_Transport compy_transport_srtcp(
    Compy_Transport inner, Compy_SrtpSuite suite,
    const Compy_SrtpKeyMaterial *key) {
    assert(inner.self && inner.vptr);
    assert(key);

    Compy_SrtcpTransport *self = malloc(sizeof *self);
    assert(self);

    self->inner = inner;
    self->suite = suite;
    self->srtcp_index = 0;

    switch (suite) {
    case Compy_SrtpSuite_AES_CM_128_HMAC_SHA1_80:
        self->auth_tag_len = SRTP_AUTH_TAG_80;
        break;
    case Compy_SrtpSuite_AES_CM_128_HMAC_SHA1_32:
        self->auth_tag_len = SRTP_AUTH_TAG_32;
        break;
    }

    srtcp_derive_keys(self, key);

    return DYN(Compy_SrtcpTransport, Compy_Transport, self);
}

static void Compy_SrtcpTransport_drop(VSelf) {
    VSELF(Compy_SrtcpTransport);
    assert(self);

    VCALL_SUPER(self->inner, Compy_Droppable, drop);
    volatile uint8_t *p;
    p = self->session_key;
    for (size_t i = 0; i < sizeof self->session_key; i++)
        p[i] = 0;
    p = self->session_salt;
    for (size_t i = 0; i < sizeof self->session_salt; i++)
        p[i] = 0;
    p = self->auth_key;
    for (size_t i = 0; i < sizeof self->auth_key; i++)
        p[i] = 0;
    free(self);
}

impl(Compy_Droppable, Compy_SrtcpTransport);

static int Compy_SrtcpTransport_transmit(VSelf, Compy_IoVecSlice bufs) {
    VSELF(Compy_SrtcpTransport);
    assert(self);

    /* Coalesce iovecs */
    size_t total = Compy_IoVecSlice_len(bufs);
    /* Need room for: packet + SRTCP index (4) + auth tag */
    if (total + 4 + (size_t)self->auth_tag_len > SRTP_MAX_PACKET_SIZE) {
        return -1;
    }

    uint8_t packet[SRTP_MAX_PACKET_SIZE];
    size_t offset = 0;
    for (size_t i = 0; i < bufs.len; i++) {
        memcpy(packet + offset, bufs.ptr[i].iov_base, bufs.ptr[i].iov_len);
        offset += bufs.ptr[i].iov_len;
    }

    /* RTCP header is at least 8 bytes (V=2, PT, length, SSRC) */
    if (total < 8) {
        return -1;
    }

    /* Extract SSRC from RTCP header (bytes 4-7) */
    uint32_t ssrc;
    memcpy(&ssrc, packet + 4, 4);
    ssrc = ntohl(ssrc);

    /*
     * SRTCP encrypts everything after the first 8 bytes (header + SSRC).
     * The encryption IV uses the SRTCP index instead of ROC+seq.
     */
    srtp_encrypt_payload(
        self->session_key, self->session_salt, ssrc, self->srtcp_index,
        packet + 8, total - 8);

    /* Append SRTCP index with E-flag (bit 31 = 1 means encrypted) */
    uint32_t srtcp_index_be = htonl(self->srtcp_index | 0x80000000);
    memcpy(packet + total, &srtcp_index_be, 4);
    total += 4;

    /* Auth tag covers: header + encrypted payload + SRTCP index */
    uint8_t hmac_out[20];
    compy_crypto_srtp_ops.hmac_sha1(
        self->auth_key, sizeof self->auth_key, packet, total, hmac_out);

    memcpy(packet + total, hmac_out, (size_t)self->auth_tag_len);
    total += (size_t)self->auth_tag_len;

    self->srtcp_index++;

    const Compy_IoVecSlice encrypted =
        (Compy_IoVecSlice)Slice99_typed_from_array((struct iovec[]){
            {.iov_base = packet, .iov_len = total},
        });

    return VCALL(self->inner, transmit, encrypted);
}

static bool Compy_SrtcpTransport_is_full(VSelf) {
    VSELF(Compy_SrtcpTransport);
    return VCALL(self->inner, is_full);
}

impl(Compy_Transport, Compy_SrtcpTransport);

/* --- SRTP/SRTCP receive-side decryption --- */

/*
 * Replay protection window (RFC 3711 Section 3.3.2).
 *
 * A 64-bit bitmask tracks which of the last 64 packet indices have
 * been seen. Packets older than the window are rejected. Packets
 * within the window are checked against the bitmask.
 */
#define SRTP_REPLAY_WINDOW_SIZE 64

struct Compy_SrtpRecvCtx {
    /* SRTP keys */
    uint8_t srtp_key[16];
    uint8_t srtp_salt[14];
    uint8_t srtp_auth_key[20];
    /* SRTCP keys */
    uint8_t srtcp_key[16];
    uint8_t srtcp_salt[14];
    uint8_t srtcp_auth_key[20];
    /* SRTP state */
    uint32_t roc;
    uint16_t s_l;     /* highest seq number seen */
    bool initialized; /* false until first packet */
    uint64_t replay_window;
    /* SRTCP state */
    uint32_t srtcp_highest_index;
    /* Common */
    int auth_tag_len;
};

Compy_SrtpRecvCtx *
compy_srtp_recv_new(Compy_SrtpSuite suite, const Compy_SrtpKeyMaterial *key) {
    assert(key);

    Compy_SrtpRecvCtx *self = malloc(sizeof *self);
    assert(self);

    /* Derive SRTP session keys */
    srtp_kdf(
        key->master_key, key->master_salt, LABEL_SRTP_ENCRYPTION,
        self->srtp_key, 16);
    srtp_kdf(
        key->master_key, key->master_salt, LABEL_SRTP_SALT, self->srtp_salt,
        14);
    srtp_kdf(
        key->master_key, key->master_salt, LABEL_SRTP_AUTH, self->srtp_auth_key,
        20);

    /* Derive SRTCP session keys */
    srtp_kdf(
        key->master_key, key->master_salt, LABEL_SRTCP_ENCRYPTION,
        self->srtcp_key, 16);
    srtp_kdf(
        key->master_key, key->master_salt, LABEL_SRTCP_SALT, self->srtcp_salt,
        14);
    srtp_kdf(
        key->master_key, key->master_salt, LABEL_SRTCP_AUTH,
        self->srtcp_auth_key, 20);

    self->roc = 0;
    self->s_l = 0;
    self->initialized = false;
    self->replay_window = 0;
    self->srtcp_highest_index = 0;

    switch (suite) {
    case Compy_SrtpSuite_AES_CM_128_HMAC_SHA1_80:
        self->auth_tag_len = SRTP_AUTH_TAG_80;
        break;
    case Compy_SrtpSuite_AES_CM_128_HMAC_SHA1_32:
        self->auth_tag_len = SRTP_AUTH_TAG_32;
        break;
    }

    return self;
}

void compy_srtp_recv_free(Compy_SrtpRecvCtx *ctx) {
    if (ctx) {
        volatile uint8_t *p;
        p = ctx->srtp_key;
        for (size_t i = 0; i < 16; i++)
            p[i] = 0;
        p = ctx->srtp_salt;
        for (size_t i = 0; i < 14; i++)
            p[i] = 0;
        p = ctx->srtp_auth_key;
        for (size_t i = 0; i < 20; i++)
            p[i] = 0;
        p = ctx->srtcp_key;
        for (size_t i = 0; i < 16; i++)
            p[i] = 0;
        p = ctx->srtcp_salt;
        for (size_t i = 0; i < 14; i++)
            p[i] = 0;
        p = ctx->srtcp_auth_key;
        for (size_t i = 0; i < 20; i++)
            p[i] = 0;
        free(ctx);
    }
}

/*
 * ROC estimation per RFC 3711 Section 3.3.1.
 *
 * Estimates the sender's ROC from the received 16-bit sequence number
 * and the receiver's current state (s_l = highest seq seen, roc).
 * Returns the estimated ROC for this packet.
 */
static uint32_t
estimate_roc(const Compy_SrtpRecvCtx *ctx, uint16_t seq, uint32_t *out_index) {
    uint32_t v;

    if (!ctx->initialized) {
        v = 0;
    } else if (ctx->s_l < 0x8000) {
        if (seq - ctx->s_l > 0x8000) {
            /* Seq wrapped backward — use previous ROC */
            v = (ctx->roc == 0) ? 0 : ctx->roc - 1;
        } else {
            v = ctx->roc;
        }
    } else {
        if (ctx->s_l - 0x8000 > seq) {
            /* Seq wrapped forward — next ROC */
            v = ctx->roc + 1;
        } else {
            v = ctx->roc;
        }
    }

    *out_index = v * 65536 + seq;
    return v;
}

/*
 * Replay check per RFC 3711 Section 3.3.2.
 *
 * Returns 0 if the packet is acceptable, -1 if it's a replay.
 * Does NOT update the window — call replay_update after successful auth.
 */
static int replay_check(const Compy_SrtpRecvCtx *ctx, uint32_t index) {
    if (!ctx->initialized) {
        return 0;
    }

    uint32_t highest = ctx->roc * 65536 + ctx->s_l;

    if (index > highest) {
        return 0; /* New packet, ahead of window */
    }

    uint32_t delta = highest - index;
    if (delta >= SRTP_REPLAY_WINDOW_SIZE) {
        return -1; /* Too old */
    }

    if (ctx->replay_window & ((uint64_t)1 << delta)) {
        return -1; /* Already seen */
    }

    return 0;
}

static void replay_update(Compy_SrtpRecvCtx *ctx, uint16_t seq, uint32_t v) {
    uint32_t index = v * 65536 + seq;

    if (!ctx->initialized) {
        ctx->s_l = seq;
        ctx->roc = v;
        ctx->replay_window = 1;
        ctx->initialized = true;
        return;
    }

    uint32_t highest = ctx->roc * 65536 + ctx->s_l;

    if (index > highest) {
        uint32_t shift = index - highest;
        if (shift < SRTP_REPLAY_WINDOW_SIZE) {
            ctx->replay_window <<= shift;
        } else {
            ctx->replay_window = 0;
        }
        ctx->replay_window |= 1;
        ctx->s_l = seq;
        ctx->roc = v;
    } else {
        uint32_t delta = highest - index;
        ctx->replay_window |= ((uint64_t)1 << delta);
    }
}

int compy_srtp_recv_unprotect(
    Compy_SrtpRecvCtx *ctx, uint8_t *data, size_t *len) {
    assert(ctx);
    assert(data);
    assert(len);

    if (*len < 12 + (size_t)ctx->auth_tag_len) {
        return -1;
    }

    size_t encrypted_len = *len - (size_t)ctx->auth_tag_len;

    /* Extract SSRC and seq */
    uint32_t ssrc;
    uint16_t seq;
    memcpy(&ssrc, data + 8, 4);
    memcpy(&seq, data + 2, 2);
    ssrc = ntohl(ssrc);
    seq = ntohs(seq);

    /* Estimate ROC and compute packet index */
    uint32_t index;
    uint32_t v = estimate_roc(ctx, seq, &index);

    /* Replay check (before auth, to reject cheap) */
    if (replay_check(ctx, index) != 0) {
        return -1;
    }

    /* Verify authentication tag */
    uint8_t auth_input[SRTP_MAX_PACKET_SIZE + 4];
    memcpy(auth_input, data, encrypted_len);
    uint32_t roc_be = htonl(v);
    memcpy(auth_input + encrypted_len, &roc_be, 4);

    uint8_t expected_tag[20];
    compy_crypto_srtp_ops.hmac_sha1(
        ctx->srtp_auth_key, 20, auth_input, encrypted_len + 4, expected_tag);

    /* Constant-time comparison */
    uint8_t diff = 0;
    for (int i = 0; i < ctx->auth_tag_len; i++) {
        diff |= expected_tag[i] ^ data[encrypted_len + i];
    }
    if (diff != 0) {
        return -1;
    }

    /* Auth passed — update replay window */
    replay_update(ctx, seq, v);

    /* Decrypt payload */
    uint8_t cc = data[0] & 0x0F;
    size_t header_len = 12 + (size_t)cc * 4;
    if (header_len > encrypted_len) {
        return -1;
    }

    srtp_encrypt_payload(
        ctx->srtp_key, ctx->srtp_salt, ssrc, index, data + header_len,
        encrypted_len - header_len);

    *len = encrypted_len;
    return 0;
}

int compy_srtcp_recv_unprotect(
    Compy_SrtpRecvCtx *ctx, uint8_t *data, size_t *len) {
    assert(ctx);
    assert(data);
    assert(len);

    /* Minimum: 8 (RTCP header) + 4 (SRTCP index) + auth_tag */
    if (*len < 8 + 4 + (size_t)ctx->auth_tag_len) {
        return -1;
    }

    size_t auth_start = *len - (size_t)ctx->auth_tag_len;
    size_t index_start = auth_start - 4;

    /* Verify authentication tag (covers everything up to but not including
     * tag) */
    uint8_t expected_tag[20];
    compy_crypto_srtp_ops.hmac_sha1(
        ctx->srtcp_auth_key, 20, data, auth_start, expected_tag);

    uint8_t diff = 0;
    for (int i = 0; i < ctx->auth_tag_len; i++) {
        diff |= expected_tag[i] ^ data[auth_start + i];
    }
    if (diff != 0) {
        return -1;
    }

    /* Extract SRTCP index and E-flag */
    uint32_t srtcp_index;
    memcpy(&srtcp_index, data + index_start, 4);
    srtcp_index = ntohl(srtcp_index);
    bool encrypted = (srtcp_index & 0x80000000) != 0;
    srtcp_index &= 0x7FFFFFFF;

    /* Reject replayed SRTCP packets (must be monotonically increasing) */
    if (srtcp_index < ctx->srtcp_highest_index) {
        return -1;
    }
    ctx->srtcp_highest_index = srtcp_index + 1;

    /* Extract SSRC from RTCP header (bytes 4-7) */
    uint32_t ssrc;
    memcpy(&ssrc, data + 4, 4);
    ssrc = ntohl(ssrc);

    /* Decrypt if E-flag is set */
    if (encrypted) {
        srtp_encrypt_payload(
            ctx->srtcp_key, ctx->srtcp_salt, ssrc, srtcp_index, data + 8,
            index_start - 8);
    }

    /* Strip SRTCP index and auth tag */
    *len = index_start;
    return 0;
}

/* --- SRTP key management helpers --- */

int compy_srtp_generate_key(Compy_SrtpKeyMaterial *key) {
    assert(key);

    if (compy_crypto_srtp_ops.random_bytes(key->master_key, 16) != 0) {
        return -1;
    }
    if (compy_crypto_srtp_ops.random_bytes(key->master_salt, 14) != 0) {
        return -1;
    }
    return 0;
}

int compy_srtp_format_crypto_attr(
    char *buf, size_t buf_len, int tag, Compy_SrtpSuite suite,
    const Compy_SrtpKeyMaterial *key) {
    assert(buf);
    assert(key);

    /* Concatenate key + salt (30 bytes) and base64 encode */
    uint8_t key_salt[30];
    memcpy(key_salt, key->master_key, 16);
    memcpy(key_salt + 16, key->master_salt, 14);

    char b64[44]; /* 30 bytes → 40 base64 chars + null */
    ssize_t b64_len = compy_base64_encode(key_salt, 30, b64, sizeof b64);
    if (b64_len < 0) {
        return -1;
    }

    const char *suite_str;
    switch (suite) {
    case Compy_SrtpSuite_AES_CM_128_HMAC_SHA1_80:
        suite_str = "AES_CM_128_HMAC_SHA1_80";
        break;
    case Compy_SrtpSuite_AES_CM_128_HMAC_SHA1_32:
        suite_str = "AES_CM_128_HMAC_SHA1_32";
        break;
    default:
        return -1;
    }

    int ret = snprintf(buf, buf_len, "%d %s inline:%s", tag, suite_str, b64);
    return ret >= (int)buf_len ? -1 : ret;
}

int compy_srtp_parse_crypto_attr(
    const char *attr_value, Compy_SrtpSuite *suite,
    Compy_SrtpKeyMaterial *key) {
    assert(attr_value);
    assert(suite);
    assert(key);

    int tag;
    char suite_str[64] = {0};
    char b64_key[64] = {0};

    if (sscanf(attr_value, "%d %63s inline:%63s", &tag, suite_str, b64_key) !=
        3) {
        return -1;
    }

    if (strcmp(suite_str, "AES_CM_128_HMAC_SHA1_80") == 0) {
        *suite = Compy_SrtpSuite_AES_CM_128_HMAC_SHA1_80;
    } else if (strcmp(suite_str, "AES_CM_128_HMAC_SHA1_32") == 0) {
        *suite = Compy_SrtpSuite_AES_CM_128_HMAC_SHA1_32;
    } else {
        return -1;
    }

    uint8_t key_salt[30];
    ssize_t decoded = compy_base64_decode(
        b64_key, strlen(b64_key), key_salt, sizeof key_salt);
    if (decoded != 30) {
        return -1;
    }

    memcpy(key->master_key, key_salt, 16);
    memcpy(key->master_salt, key_salt + 16, 14);
    return 0;
}
