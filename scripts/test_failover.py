import requests
import time
import sys

def test_failover(url):
    print(f"Monitoring {url} for failover...")
    print("Please manually stop a backend container (e.g. 'docker stop btp-backend-1-1') in another terminal.")
    
    consecutive_errors = 0
    start_error_time = 0
    
    last_backend = None
    
    while True:
        try:
            resp = requests.get(url, timeout=2)
            if resp.status_code == 200:
                data = resp.json()
                backend = data.get("backend", "unknown")
                if consecutive_errors > 0:
                    recovery_time = time.time() - start_error_time
                    print(f"\n[SUCCESS] Traffic recovered! Re-routed to {backend}.")
                    print(f"Downtime observed: {recovery_time:.2f} seconds")
                    break
                print(f"[{time.strftime('%X')}] OK -> {backend}", end='\r')
                last_backend = backend
                time.sleep(0.5)
            else:
                if consecutive_errors == 0:
                    start_error_time = time.time()
                    print(f"\n[ERROR] Request failed with {resp.status_code}. Starting timer...")
                consecutive_errors += 1
                time.sleep(0.2)
        except Exception as e:
            if consecutive_errors == 0:
                start_error_time = time.time()
                print(f"\n[ERROR] Request exception: {e}. Starting timer...")
            consecutive_errors += 1
            time.sleep(0.2)

if __name__ == "__main__":
    url = "http://localhost:8080/"
    if len(sys.argv) > 1:
        url = sys.argv[1]
    test_failover(url)
