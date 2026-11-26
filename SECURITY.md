# Security Policy

## Reporting a vulnerability

Please report security issues privately through GitHub:
**Security → Report a vulnerability** on this repository
(<https://github.com/gtxaspec/compy/security/advisories/new>).

Do not open a public issue for a vulnerability. Include what you can of:

- the affected component (library under `src/`, or the example server in
  `examples/`) and the commit you tested;
- the request sequence or input that triggers it, and what happens;
- the build you used (compiler, sanitizers, `-DNDEBUG` or not).

You should get an acknowledgement within a few days. Fixes land on `main`
with a regression test, and the advisory credits the reporter unless you
ask otherwise.

## Scope

The library (`src/`, `include/`) is in scope. The example server in
`examples/` is demonstration code, but issues in it are welcome too, since
servers are often built from it.
