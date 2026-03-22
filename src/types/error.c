#include <compy/types/error.h>

const char *Compy_ParseType_str(Compy_ParseType self) {
    switch (self) {
    case Compy_ParseType_Int:
        return "Integer";
    case Compy_ParseType_Ident:
        return "Identifier";
    case Compy_ParseType_HeaderName:
        return "Header name";
    default:
        return "Unknown";
    }
}

#define MAX_STR 10
#define TRUNCATE_STR(str)                                                      \
    ((str).len <= MAX_STR ? (str) : CharSlice99_sub((str), 0, MAX_STR))

int Compy_ParseError_print(Compy_ParseError self, Compy_Writer w) {
    assert(w.self && w.vptr);

    match(self) {
        of(Compy_ParseError_ContentLength, value) {
            return COMPY_WRITE_SLICES(
                w, {
                       CharSlice99_from_str("Invalid Content-Length `"),
                       TRUNCATE_STR(*value),
                       CharSlice99_from_str("`"),
                   });
        }
        of(Compy_ParseError_StrMismatch, expected, actual) {
            return COMPY_WRITE_SLICES(
                w, {
                       CharSlice99_from_str("String mismatch: expected `"),
                       TRUNCATE_STR(*expected),
                       CharSlice99_from_str("`, found `"),
                       TRUNCATE_STR(*actual),
                       CharSlice99_from_str("`"),
                   });
        }
        of(Compy_ParseError_TypeMismatch, kind, str) {
            return COMPY_WRITE_SLICES(
                w, {
                       CharSlice99_from_str("Type mismatch: expected "),
                       CharSlice99_from_str((char *)Compy_ParseType_str(*kind)),
                       CharSlice99_from_str(", found `"),
                       TRUNCATE_STR(*str),
                       CharSlice99_from_str("`"),
                   });
        }
        of(Compy_ParseError_HeaderMapOverflow) {
            return VCALL(
                w, write,
                CharSlice99_from_str(
                    "Not enough space left in the header map"));
        }
        of(Compy_ParseError_MissingCSeq) {
            return VCALL(w, write, CharSlice99_from_str("Missing CSeq"));
        }
        of(Compy_ParseError_InvalidCSeq, value) {
            return COMPY_WRITE_SLICES(
                w, {
                       CharSlice99_from_str("Invalid CSeq `"),
                       TRUNCATE_STR(*value),
                       CharSlice99_from_str("`"),
                   });
        }
    }

    return -1;
}

#undef MAX_STR
#undef TRUNCATE_STR

bool Compy_ParseStatus_is_complete(Compy_ParseStatus self) {
    return MATCHES(self, Compy_ParseStatus_Complete);
}

bool Compy_ParseStatus_is_partial(Compy_ParseStatus self) {
    return MATCHES(self, Compy_ParseStatus_Partial);
}

Compy_ParseResult Compy_ParseResult_partial(void) {
    return Compy_ParseResult_Success(Compy_ParseStatus_Partial());
}

Compy_ParseResult Compy_ParseResult_complete(size_t offset) {
    return Compy_ParseResult_Success(Compy_ParseStatus_Complete(offset));
}

bool Compy_ParseResult_is_success(Compy_ParseResult self) {
    return MATCHES(self, Compy_ParseResult_Success);
}

bool Compy_ParseResult_is_failure(Compy_ParseResult self) {
    return !Compy_ParseResult_is_success(self);
}

bool Compy_ParseResult_is_partial(Compy_ParseResult self) {
    // Used to workaround `-Wreturn-type`.
    bool result = true;

    match(self) {
        of(Compy_ParseResult_Success, status) result =
            Compy_ParseStatus_is_partial(*status);
        otherwise result = false;
    }

    return result;
}

bool Compy_ParseResult_is_complete(Compy_ParseResult self) {
    // Used to workaround `-Wreturn-type`.
    bool result = true;

    match(self) {
        of(Compy_ParseResult_Success, status) result =
            Compy_ParseStatus_is_complete(*status);
        otherwise result = false;
    }

    return result;
}
