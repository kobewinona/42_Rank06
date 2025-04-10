import socket
import time
import os
import fcntl

s = socket.socket()
s.connect(("127.0.0.1", 8090))

fcntl.fcntl(s, fcntl.F_SETFL, os.O_NONBLOCK)

time.sleep(3)

try:
    while True:
        try:
            data = s.recv(1024)
            if not data:
                break
            print("RECV:", data.decode(), end="", flush=True)
        except BlockingIOError:
            pass
        time.sleep(0.2)
except KeyboardInterrupt:
    pass

s.close()
