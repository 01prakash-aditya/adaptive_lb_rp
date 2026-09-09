import requests
import concurrent.futures
import time
import sys

def fire_request(url):
    try:
        start = time.time()
        resp = requests.get(url, timeout=5)
        return resp.status_code, time.time() - start
    except Exception as e:
        return 0, 0

def test_rate_limit(url, num_requests=50, concurrency=10):
    print(f"Sending {num_requests} requests with concurrency {concurrency} to test rate limiting...")
    
    results = {200: 0, 429: 0, "other": 0}
    
    with concurrent.futures.ThreadPoolExecutor(max_workers=concurrency) as executor:
        futures = [executor.submit(fire_request, url) for _ in range(num_requests)]
        for future in concurrent.futures.as_completed(futures):
            status, _ = future.result()
            if status == 200:
                results[200] += 1
            elif status == 429:
                results[429] += 1
            else:
                results["other"] += 1

    print("\n--- Rate Limit Results ---")
    print(f"200 OK: {results[200]}")
    print(f"429 Too Many Requests: {results[429]}")
    print(f"Other Errors: {results['other']}")
    
    if results[429] > 0:
        print("\n[SUCCESS] Rate limiting correctly intercepted burst traffic.")
    else:
        print("\n[WARNING] No 429s observed. Limit might be too high or burst too small.")

if __name__ == "__main__":
    url = "http://localhost:8080/"
    if len(sys.argv) > 1:
        url = sys.argv[1]
    test_rate_limit(url, 200, 50)
