#include <compy/controller.h>

#include <assert.h>

void compy_dispatch(
    Compy_Writer conn, Compy_Controller controller,
    const Compy_Request *restrict req) {
    assert(conn.self && conn.vptr);
    assert(controller.self && controller.vptr);
    assert(req);

    Compy_Context *ctx = Compy_Context_new(conn, req->cseq);

    if (VCALL(controller, before, ctx, req) == Compy_ControlFlow_Break) {
        goto after;
    }

    const Compy_Method method = req->start_line.method,
                       options = COMPY_METHOD_OPTIONS,
                       describe = COMPY_METHOD_DESCRIBE,
                       setup = COMPY_METHOD_SETUP, play = COMPY_METHOD_PLAY,
                       pause_m = COMPY_METHOD_PAUSE,
                       teardown = COMPY_METHOD_TEARDOWN,
                       get_parameter = COMPY_METHOD_GET_PARAMETER;

    if (Compy_Method_eq(&method, &options)) {
        VCALL(controller, options, ctx, req);
    } else if (Compy_Method_eq(&method, &describe)) {
        VCALL(controller, describe, ctx, req);
    } else if (Compy_Method_eq(&method, &setup)) {
        VCALL(controller, setup, ctx, req);
    } else if (Compy_Method_eq(&method, &play)) {
        VCALL(controller, play, ctx, req);
    } else if (Compy_Method_eq(&method, &pause_m)) {
        VCALL(controller, pause_method, ctx, req);
    } else if (Compy_Method_eq(&method, &teardown)) {
        VCALL(controller, teardown, ctx, req);
    } else if (Compy_Method_eq(&method, &get_parameter)) {
        VCALL(controller, get_parameter, ctx, req);
    } else {
        VCALL(controller, unknown, ctx, req);
    }

after:
    VCALL(controller, after, Compy_Context_get_ret(ctx), ctx, req);

    VTABLE(Compy_Context, Compy_Droppable).drop(ctx);
}
