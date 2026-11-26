#!/usr/bin/env python3
"""Out-of-order RTSP request sequences against the example server.

Each sequence runs against a fresh server process. After it the server must
still be running, must answer a new client, and must exit cleanly on SIGINT
with no sanitizer report (build it with -fsanitize=address,undefined to catch
leaks and use-after-free, not just crashes).

usage: out_of_order.py <path/to/server> <media dir>
"""

import os
import re
import signal
import socket
import subprocess
import sys
import time

SERVER, MEDIA = sys.argv[1], sys.argv[2]
PORT = 18554
TCP = "Transport: RTP/AVP/TCP;unicast;interleaved={}-{}\r\n"


class Peer:
    def __init__(self):
        self.sock = socket.create_connection(("127.0.0.1", PORT), timeout=3)
        self.cseq = 0
        self.buf = b""

    def req(self, method, path, extra=""):
        self.cseq += 1
        msg = f"{method} rtsp://127.0.0.1:{PORT}{path} RTSP/1.0\r\nCSeq: {self.cseq}\r\n{extra}\r\n"
        self.sock.sendall(msg.encode())
        return self._response()

    def _response(self):
        # interleaved RTP may arrive before or after the reply: skip it
        deadline = time.time() + 3
        while time.time() < deadline:
            m = re.search(rb"RTSP/1\.0 (\d+)[^\r]*\r\n(.*?)\r\n\r\n", self.buf, re.S)
            if m:
                self.buf = self.buf[m.end():]
                sess = re.search(rb"Session: *(\d+)", m.group(2))
                return int(m.group(1)), sess.group(1).decode() if sess else None
            try:
                data = self.sock.recv(65536)
            except socket.timeout:
                break
            if not data:
                return None, None
            self.buf += data
        return None, None

    def close(self):
        self.sock.close()


def setup(p, path, ch, session=None):
    extra = TCP.format(ch, ch + 1) + (f"Session: {session}\r\n" if session else "")
    return p.req("SETUP", path, extra)


# name -> (steps, expect): each step returns nothing; the checks are afterwards
def seq_control(p):
    _, s = setup(p, "/video", 0)
    assert p.req("PLAY", "/", f"Session: {s}\r\n")[0] == 200
    assert p.req("TEARDOWN", "/", f"Session: {s}\r\n")[0] == 200


def seq_teardown_releases(p):
    _, s = setup(p, "/video", 0)
    p.req("PLAY", "/", f"Session: {s}\r\n")
    assert p.req("TEARDOWN", "/", f"Session: {s}\r\n")[0] == 200
    code = p.req("PLAY", "/", f"Session: {s}\r\n")[0]
    assert code == 454, f"PLAY after TEARDOWN got {code}, not 454"


def seq_teardown_before_play(p):
    _, s = setup(p, "/video", 0)
    p.req("TEARDOWN", "/", f"Session: {s}\r\n")


def seq_backchannel_play(p):
    _, s = setup(p, "/audioback", 4)
    p.req("PLAY", "/", f"Session: {s}\r\n")


def seq_play_session_zero(p):
    p.req("PLAY", "/", "Session: 0\r\n")
    p.req("TEARDOWN", "/", "Session: 0\r\n")


def seq_play_twice_disconnect(p):
    _, s = setup(p, "/video", 0)
    p.req("PLAY", "/", f"Session: {s}\r\n")
    p.req("PLAY", "/", f"Session: {s}\r\n")
    time.sleep(0.3)


def seq_setup_play_setup_play(p):
    _, s = setup(p, "/video", 0)
    p.req("PLAY", "/", f"Session: {s}\r\n")
    setup(p, "/video", 0, s)
    p.req("PLAY", "/", f"Session: {s}\r\n")
    time.sleep(0.3)


def seq_setup_without_play(p):
    setup(p, "/video", 0)
    setup(p, "/audio", 2)


def seq_malformed_session(p):
    setup(p, "/video", 0, "not-a-number")


SEQUENCES = [
    seq_control, seq_teardown_releases, seq_teardown_before_play, seq_backchannel_play, seq_play_session_zero,
    seq_play_twice_disconnect, seq_setup_play_setup_play, seq_setup_without_play,
    seq_malformed_session,
]


def run(seq):
    env = dict(os.environ, ASAN_OPTIONS="detect_leaks=1:abort_on_error=0",
               UBSAN_OPTIONS="print_stacktrace=1:halt_on_error=1")
    srv = subprocess.Popen(
        [SERVER, "-p", str(PORT), "-v", f"{MEDIA}/video.h264", "-a", f"{MEDIA}/audio.g711a"],
        stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, env=env)
    try:
        for _ in range(50):
            try:
                socket.create_connection(("127.0.0.1", PORT), timeout=0.2).close()
                break
            except OSError:
                time.sleep(0.1)
        problems = []
        p = Peer()
        try:
            seq(p)
        except AssertionError as e:
            problems.append(f"assertion: {e}")
        except OSError:
            pass  # a dropped connection is judged by the server's state below
        p.close()
        time.sleep(0.3)
        if srv.poll() is not None:
            problems.append(f"server exited ({srv.returncode})")
        else:
            q = Peer()
            if q.req("OPTIONS", "/")[0] != 200:
                problems.append("next client got no 200")
            q.close()
            srv.send_signal(signal.SIGINT)
            try:
                srv.wait(10)
            except subprocess.TimeoutExpired:
                srv.kill()
                problems.append("did not exit on SIGINT")
        err = srv.stderr.read().decode(errors="replace")
        for marker in ("AddressSanitizer", "LeakSanitizer", "runtime error:"):
            if marker in err:
                problems.append(marker + ": " + next(l for l in err.splitlines() if marker in l)[:120])
        return problems
    finally:
        if srv.poll() is None:
            srv.kill()


def main():
    failed = 0
    for seq in SEQUENCES:
        problems = run(seq)
        name = seq.__name__[4:]
        if problems:
            failed += 1
            print(f"FAIL {name}: " + "; ".join(problems))
        else:
            print(f"ok   {name}")
    print(f"{len(SEQUENCES) - failed}/{len(SEQUENCES)} sequences passed")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
