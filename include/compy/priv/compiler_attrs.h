#pragma once

#ifdef __GNUC__
#define COMPY_PRIV_GNUC_ATTR(attr) __attribute__((attr))
#else
#define COMPY_PRIV_GNUC_ATTR(_attr)
#endif

#if defined(__GNUC__) && !defined(__clang__)
#define COMPY_PRIV_GCC_ATTR(attr) __attribute__((attr))
#else
#define COMPY_PRIV_GCC_ATTR(_attr)
#endif

#define COMPY_PRIV_MUST_USE COMPY_PRIV_GNUC_ATTR(warn_unused_result)
