#include <compy/nal.h>

#include <assert.h>

#define NAL_HEADER_DERIVE_GETTER(T, name, h264_value, h265_value)              \
    T Compy_NalHeader_##name(Compy_NalHeader self) {                     \
        T result = 0;                                                          \
                                                                               \
        match(self) {                                                          \
            of(Compy_NalHeader_H264, h) result = h264_value;                \
            of(Compy_NalHeader_H265, h) result = h265_value;                \
        }                                                                      \
                                                                               \
        return result;                                                         \
    }

NAL_HEADER_DERIVE_GETTER(uint8_t, unit_type, h->unit_type, h->unit_type)
NAL_HEADER_DERIVE_GETTER(
    size_t, size, ((void)h, COMPY_H264_NAL_HEADER_SIZE),
    ((void)h, COMPY_H265_NAL_HEADER_SIZE))
NAL_HEADER_DERIVE_GETTER(
    size_t, fu_size, ((void)h, COMPY_H264_FU_HEADER_SIZE),
    ((void)h, COMPY_H265_FU_HEADER_SIZE))

#undef NAL_HEADER_DERIVE_GETTER

#define NAL_HEADER_DERIVE_PREDICATE(fn)                                        \
    bool Compy_NalHeader_##fn(Compy_NalHeader self) {                    \
        bool result = 0;                                                       \
                                                                               \
        match(self) {                                                          \
            of(Compy_NalHeader_H264, h) result =                            \
                Compy_H264NalHeader_##fn(*h);                               \
            of(Compy_NalHeader_H265, h) result =                            \
                Compy_H265NalHeader_##fn(*h);                               \
        }                                                                      \
                                                                               \
        return result;                                                         \
    }

NAL_HEADER_DERIVE_PREDICATE(is_vps)
NAL_HEADER_DERIVE_PREDICATE(is_sps)
NAL_HEADER_DERIVE_PREDICATE(is_pps)
NAL_HEADER_DERIVE_PREDICATE(is_coded_slice_idr)
NAL_HEADER_DERIVE_PREDICATE(is_coded_slice_non_idr)

#undef NAL_HEADER_DERIVE_PREDICATE

void Compy_NalHeader_serialize(
    Compy_NalHeader self, uint8_t buffer[restrict]) {
    match(self) {
        of(Compy_NalHeader_H264, h) {
            const uint8_t repr = Compy_H264NalHeader_serialize(*h);
            buffer[0] = repr;
        }
        of(Compy_NalHeader_H265, h) {
            const uint16_t repr = Compy_H265NalHeader_serialize(*h);
            memcpy(buffer, &repr, sizeof repr);
        }
    }
}

void Compy_NalHeader_write_fu_header(
    Compy_NalHeader self, uint8_t buffer[restrict], bool is_first_fragment,
    bool is_last_fragment) {
    match(self) {
        of(Compy_NalHeader_H264, h) Compy_H264NalHeader_write_fu_header(
            *h, buffer, is_first_fragment, is_last_fragment);
        of(Compy_NalHeader_H265, h) Compy_H265NalHeader_write_fu_header(
            *h, buffer, is_first_fragment, is_last_fragment);
    }
}

// See <https://tools.ietf.org/html/rfc7798#section-4.4.3> (H.265),
// <https://tools.ietf.org/html/rfc6184#section-5.8> (H.264).
uint8_t compy_nal_fu_header(
    bool is_first_fragment, bool is_last_fragment, uint8_t unit_type) {
    /*
     * H.264:
     * +---------------+
     * |0|1|2|3|4|5|6|7|
     * +-+-+-+-+-+-+-+-+
     * |S|E|R|  Type   |
     * +---------------+
     * */

    /*
     * H.265:
     * +---------------+
     * |0|1|2|3|4|5|6|7|
     * +-+-+-+-+-+-+-+-+
     * |S|E|  FuType   |
     * +---------------+
     */
    uint8_t fu_header = (uint8_t)0b00000000;

    if (is_first_fragment) {
        fu_header |= (uint8_t)0b10000000;
    }
    if (is_last_fragment) {
        fu_header |= (uint8_t)0b01000000;
    }
    fu_header += unit_type;

    return fu_header;
}

static const uint8_t start_code_3b[] = {0x00, 0x00, 0x01},
                     start_code_4b[] = {0x00, 0x00, 0x00, 0x01};

Compy_NalStartCodeTester compy_determine_start_code(U8Slice99 data) {
    if (compy_test_start_code_3b(data)) {
        return compy_test_start_code_3b;
    }

    if (compy_test_start_code_4b(data)) {
        return compy_test_start_code_4b;
    }

    return NULL;
}

size_t compy_test_start_code_3b(U8Slice99 data) {
    if (data.len < 3) {
        return 0;
    }
    if (memcmp(data.ptr, start_code_3b, 3) == 0) {
        return 3;
    }

    return 0;
}

size_t compy_test_start_code_4b(U8Slice99 data) {
    if (data.len < 4) {
        return 0;
    }
    if (memcmp(data.ptr, start_code_4b, 4) == 0) {
        return 4;
    }

    return 0;
}
