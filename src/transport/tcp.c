#include <compy/transport.h>

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

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

static int Compy_TcpTransport_transmit(VSelf, Compy_IoVecSlice bufs) {
    VSELF(Compy_TcpTransport);
    assert(self);

    const size_t total_bytes = Compy_IoVecSlice_len(bufs);

    const uint32_t header =
        compy_interleaved_header(self->channel_id, htons(total_bytes));

    VCALL(self->w, lock);
    ssize_t ret =
        VCALL(self->w, write, CharSlice99_new((char *)&header, sizeof header));
    if (ret != sizeof header) {
        VCALL(self->w, unlock);
        return -1;
    }

    for (size_t i = 0; i < bufs.len; i++) {
        const CharSlice99 vec =
            CharSlice99_new(bufs.ptr[i].iov_base, bufs.ptr[i].iov_len);
        ret = VCALL(self->w, write, vec);
        if (ret != (ssize_t)vec.len) {
            VCALL(self->w, unlock);
            return -1;
        }
    }
    VCALL(self->w, unlock);

    return 0;
}

static bool Compy_TcpTransport_is_full(VSelf) {
    VSELF(Compy_TcpTransport);
    assert(self);

    return VCALL(self->w, filled) > self->max_buffer;
}

impl(Compy_Transport, Compy_TcpTransport);
