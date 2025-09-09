import subprocess
import statistics

NUM_CLIENTS = 50   # adjust based on your machine
clients = []

# Start all clients
for i in range(NUM_CLIENTS):
    p = subprocess.Popen(
        ["./build/NetworkClient"], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True
    )
    clients.append(p)

results = []

# Collect results from all clients
for p in clients:
    out, err = p.communicate()
    if err:
        print("Client error:", err.strip())
    if out:
        print("Client output:", out.strip())
        # Expect output like: RTT avg=0.12 ms, min=0.05 ms, max=0.30 ms
        try:
            parts = out.strip().split(",")
            avg_ms = float(parts[0].split("=")[1].split()[0])
            min_ms = float(parts[1].split("=")[1].split()[0])
            max_ms = float(parts[2].split("=")[1].split()[0])
            results.append((avg_ms, min_ms, max_ms))
        except Exception as e:
            print("Failed to parse output:", e)

# Aggregate results
if results:
    avg_rtts = [r[0] for r in results]
    min_rtts = [r[1] for r in results]
    max_rtts = [r[2] for r in results]

    print("\n=== Aggregate RTT Results ===")
    print(f"Clients: {NUM_CLIENTS}")
    print(f"Overall Avg RTT = {statistics.mean(avg_rtts):.3f} ms")
    print(f"Overall Min RTT = {min(min_rtts):.3f} ms")
    print(f"Overall Max RTT = {max(max_rtts):.3f} ms")
else:
    print("No results collected")
