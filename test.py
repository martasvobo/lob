import subprocess

NUM_CLIENTS = 50
clients = []

for i in range(NUM_CLIENTS):
    p = subprocess.Popen(["./NetworkClient"])
    clients.append(p)


for p in clients:
    p.wait()
