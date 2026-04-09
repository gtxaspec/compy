/*
 * Fuzz target for Compy_Response_parse.
 *
 * Build with: clang -fsanitize=fuzzer,address -I../include \
 *             -I../_deps/slice99-src -I../_deps/datatype99-src \
 *             -I../_deps/interface99-src -I../_deps/metalang99-src/include \
 *             fuzz_response_parse.c ../build/libcompy.a -o fuzz_response -lpthread
 *
 * Run with:   ./fuzz_response corpus_dir/
 */

#include <compy/types/response.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    Compy_Response resp = Compy_Response_uninit();
    CharSlice99 input = CharSlice99_new((char *)data, size);

    (void)Compy_Response_parse(&resp, input);

    return 0;
}
