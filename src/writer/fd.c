#include <compy/writer.h>

#include <assert.h>
#include <errno.h>
#include <unistd.h>

typedef int FdWriter;

static ssize_t FdWriter_write(VSelf, CharSlice99 data) {
    VSELF(FdWriter);
    assert(self);

    const char *p = data.ptr;
    size_t remaining = data.len;

    while (remaining > 0) {
        ssize_t n = write(*self, p, remaining);
        if (n > 0) {
            p += n;
            remaining -= (size_t)n;
        } else if (n == 0) {
            break;
        } else {
            if (errno == EINTR)
                continue;
            return -1;
        }
    }

    return (ssize_t)(data.len - remaining);
}

static void FdWriter_lock(VSelf) {
    VSELF(FdWriter);
    (void)self;
}

static void FdWriter_unlock(VSelf) {
    VSELF(FdWriter);
    (void)self;
}

static size_t FdWriter_filled(VSelf) {
    VSELF(FdWriter);
    (void)self;
    return 0;
}

static int FdWriter_vwritef(VSelf, const char *restrict fmt, va_list ap) {
    VSELF(FdWriter);

    assert(self);
    assert(fmt);

    return vdprintf(*self, fmt, ap);
}

static int FdWriter_writef(VSelf, const char *restrict fmt, ...) {
    VSELF(FdWriter);

    assert(self);
    assert(fmt);

    va_list ap;
    va_start(ap, fmt);

    const int ret = FdWriter_vwritef(self, fmt, ap);
    va_end(ap);

    return ret;
}

impl(Compy_Writer, FdWriter);

Compy_Writer compy_fd_writer(int *fd) {
    assert(fd);
    return DYN(FdWriter, Compy_Writer, fd);
}
