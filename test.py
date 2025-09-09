import subprocess
import time

NUM_CLIENTS = 50
clients = []

for i in range(NUM_CLIENTS):
    p = subprocess.Popen(["./NetworkClient"])
    clients.append(p)

time.sleep(20)  # run for 20s

for p in clients:
    p.terminate()
