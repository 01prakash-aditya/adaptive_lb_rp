import requests
import collections
import time
import sys

def test_distribution(url, num_requests=100):
    counts = collections.defaultdict(int)
    print(f"Sending {num_requests} requests to {url}...")
    
    start_time = time.time()
    for _ in range(num_requests):
        try:
            resp = requests.get(url)
            if resp.status_code == 200:
                data = resp.json()
                backend = data.get("backend", "unknown")
                counts[backend] += 1
            else:
                counts[f"error_{resp.status_code}"] += 1
        except Exception as e:
            counts["exception"] += 1
            
    elapsed = time.time() - start_time
    
    print(f"\n--- Distribution Results ---")
    print(f"Total time: {elapsed:.2f}s")
    for backend, count in sorted(counts.items()):
        print(f"{backend}: {count} hits ({count/num_requests*100:.1f}%)")

if __name__ == "__main__":
    url = "http://localhost:8080/"
    if len(sys.argv) > 1:
        url = sys.argv[1]
    test_distribution(url, 200)
