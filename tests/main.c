#include <greatest.h>

// Check that the main header compiles well.
#include <compy.h>

#define COMPY_SUITE(name)                                                      \
    extern void name(void);                                                    \
    RUN_SUITE(name)

GREATEST_MAIN_DEFS();

int main(int argc, char *argv[]) {
    GREATEST_MAIN_BEGIN();

    COMPY_SUITE(types_header_map);
    COMPY_SUITE(types_header);
    COMPY_SUITE(types_message_body);
    COMPY_SUITE(types_method);
    COMPY_SUITE(types_reason_phrase);
    COMPY_SUITE(types_request_line);
    COMPY_SUITE(types_request_uri);
    COMPY_SUITE(types_request);
    COMPY_SUITE(types_response_line);
    COMPY_SUITE(types_response);
    COMPY_SUITE(types_rtsp_version);
    COMPY_SUITE(types_sdp);
    COMPY_SUITE(types_status_code);

    COMPY_SUITE(nal_h264);
    COMPY_SUITE(nal_h265);
    COMPY_SUITE(nal);

    COMPY_SUITE(util);
    COMPY_SUITE(writer);
    COMPY_SUITE(transport);
    COMPY_SUITE(io_vec);
    COMPY_SUITE(context);
    COMPY_SUITE(controller);

    COMPY_SUITE(types_rtcp);
    COMPY_SUITE(rtcp);
    COMPY_SUITE(receiver);
    COMPY_SUITE(auth);
    COMPY_SUITE(base64);

#ifdef COMPY_HAS_TLS
    COMPY_SUITE(srtp);
    COMPY_SUITE(tls);
#endif

    GREATEST_MAIN_END();
}
