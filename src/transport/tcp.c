#include <compy/transport.h>

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>

#include <slice99.h>

#include <compy/util.h>

typedef struct {
    Compy_Writer w;
    int channel_id;
    size_t max_buffer;
} Compy_TcpTransport;

declImpl(Compy_Transport, Compy_TcpTransport);

Compy_Transport
compy_transport_tcp(Compy_Writer w, uint8_t channel_id, size_t max_buffer) {
    assert(w.self && w.vptr);

    Compy_TcpTransport *self = malloc(sizeof *self);
    assert(self);

    self->w = w;
    self->channel_id = channel_id;
    self->max_buffer = max_buffer;

    return DYN(Compy_TcpTransport, Compy_Transport, self);
}

static void Compy_TcpTransport_drop(VSelf) {
    VSELF(Compy_TcpTransport);
    assert(self);

    free(self);
}

impl(Compy_Droppable, Compy_TcpTransport);

/*
 * Stack buffer for coalesced transmits. path_mtu (≤1500) + NAL/RTP
 * headers + the 4-byte TLS interleave header stays well under 2 KB in
 * practice, so the common case avoids malloc entirely. Jumbo frames
 * fall back to the heap.
 */
#define COMPY_TCP_TX_STACK_BUF 2048

static int Compy_TcpTransport_transmit(VSelf, Compy_IoVecSlice bufs) {
    VSELF(Compy_TcpTransport);
    assert(self);

    const size_t total_bytes = Compy_IoVecSlice_len(bufs);
    const uint32_t header =
        compy_interleaved_header(self->channel_id, htons(total_bytes));
    const size_t total = sizeof header + total_bytes;

    /*
     * Coalesce interleaved header + all iovecs into one writer.write()
     * so a TLS writer emits one TLS record per RTP packet instead of
     * one per iovec. Each record carries ~29 bytes of GCM framing, so
     * the pre-coalesce pattern of 4 iovecs -> 4 records was almost
     * entirely framing overhead.
     */
    unsigned char stackbuf[COMPY_TCP_TX_STACK_BUF];
    unsigned char *buf = stackbuf;

    if (total > sizeof stackbuf) {
        buf = malloc(total);
        if (!buf) return -1;
    }

    memcpy(buf, &header, sizeof header);
    size_t pos = sizeof header;
    for (size_t i = 0; i < bufs.len; i++) {
        memcpy(buf + pos, bufs.ptr[i].iov_base, bufs.ptr[i].iov_len);
        pos += bufs.ptr[i].iov_len;
    }

    VCALL(self->w, lock);
    ssize_t ret = VCALL(self->w, write, CharSlice99_new((char *)buf, total));
    VCALL(self->w, unlock);

    if (buf != stackbuf) free(buf);
    return (ret == (ssize_t)total) ? 0 : -1;
}

static bool Compy_TcpTransport_is_full(VSelf) {
    VSELF(Compy_TcpTransport);
    assert(self);

    return VCALL(self->w, filled) > self->max_buffer;
}

impl(Compy_Transport, Compy_TcpTransport);
