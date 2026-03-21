#include <compy/nal/h264.h>

#include <compy/nal.h>

Compy_H264NalHeader Compy_H264NalHeader_parse(uint8_t byte_header) {
    return (Compy_H264NalHeader){
        .forbidden_zero_bit = (byte_header & 0b10000000) >> 7 == 1,
        .ref_idc = (byte_header & 0b01100000) >> 5,
        .unit_type = (byte_header & 0b00011111),
    };
}

uint8_t Compy_H264NalHeader_serialize(Compy_H264NalHeader self) {
    return (self.forbidden_zero_bit ? 0b10000000 : 0b00000000) |
           (self.ref_idc << 5) | (self.unit_type);
}

bool Compy_H264NalHeader_is_vps(Compy_H264NalHeader self) {
    (void)self;
    return false;
}

bool Compy_H264NalHeader_is_sps(Compy_H264NalHeader self) {
    return COMPY_H264_NAL_UNIT_SPS == self.unit_type;
}

bool Compy_H264NalHeader_is_pps(Compy_H264NalHeader self) {
    return COMPY_H264_NAL_UNIT_PPS == self.unit_type;
}

bool Compy_H264NalHeader_is_coded_slice_idr(Compy_H264NalHeader self) {
    return COMPY_H264_NAL_UNIT_CODED_SLICE_IDR == self.unit_type;
}

bool Compy_H264NalHeader_is_coded_slice_non_idr(
    Compy_H264NalHeader self) {
    return COMPY_H264_NAL_UNIT_CODED_SLICE_NON_IDR == self.unit_type;
}

void Compy_H264NalHeader_write_fu_header(
    Compy_H264NalHeader self, uint8_t buffer[restrict],
    bool is_first_fragment, bool is_last_fragment) {
    uint8_t fu_identifier = (uint8_t)0b01111100; // 0, nal_ref_idc, FU-A (28)

    if ((self.ref_idc & 0b00000010) == 0) {
        fu_identifier &= 0b00111111;
    }
    if ((self.ref_idc & 0b00000001) == 0) {
        fu_identifier &= 0b01011111;
    }

    const uint8_t fu_header = compy_nal_fu_header(
        is_first_fragment, is_last_fragment, self.unit_type);

    buffer = SLICE99_APPEND(buffer, fu_identifier);
    buffer = SLICE99_APPEND(buffer, fu_header);
}
