/*
 * Fuzz target for Compy_Request_parse.
 *
 * Build with: clang -fsanitize=fuzzer,address -I../include \
 *             -I../_deps/slice99-src -I../_deps/datatype99-src \
 *             -I../_deps/interface99-src -I../_deps/metalang99-src/include \
 *             fuzz_request_parse.c ../build/libcompy.a -o fuzz_request -lpthread
 *
 * Run with:   ./fuzz_request corpus_dir/
 */

#include <compy/types/request.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    Compy_Request req = Compy_Request_uninit();
    CharSlice99 input = CharSlice99_new((char *)data, size);

    (void)Compy_Request_parse(&req, input);

    return 0;
}
