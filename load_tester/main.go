package main

import (
	"flag"
	"fmt"
	"io"
	"net/http"
	"sort"
	"sync"
	"time"
)

func main() {
	url := flag.String("url", "http://localhost:8080/", "Target URL")
	concurrency := flag.Int("concurrency", 10, "Number of concurrent workers")
	duration := flag.Int("duration", 5, "Duration in seconds")
	flag.Parse()

	fmt.Printf("Running load test: %s with %d concurrency for %d seconds\n", *url, *concurrency, *duration)

	var wg sync.WaitGroup
	var mu sync.Mutex
	latencies := make([]float64, 0)
	errors := 0
	requests := 0

	endTime := time.Now().Add(time.Duration(*duration) * time.Second)

	for i := 0; i < *concurrency; i++ {
		wg.Add(1)
		go func() {
			defer wg.Done()
			client := &http.Client{Timeout: 2 * time.Second}

			for time.Now().Before(endTime) {
				start := time.Now()
				resp, err := client.Get(*url)
				latency := time.Since(start).Seconds() * 1000.0

				mu.Lock()
				requests++
				if err != nil || resp.StatusCode >= 400 {
					errors++
				}
				latencies = append(latencies, latency)
				mu.Unlock()

				if resp != nil {
					io.Copy(io.Discard, resp.Body)
					resp.Body.Close()
				}
			}
		}()
	}

	wg.Wait()

	sort.Float64s(latencies)

	fmt.Println("\n--- Load Test Results ---")
	fmt.Printf("Total Requests: %d\n", requests)
	fmt.Printf("Errors:         %d\n", errors)
	fmt.Printf("RPS:            %.2f\n", float64(requests)/float64(*duration))

	if len(latencies) > 0 {
		p50 := latencies[int(float64(len(latencies))*0.50)]
		p95 := latencies[int(float64(len(latencies))*0.95)]
		p99 := latencies[int(float64(len(latencies))*0.99)]
		fmt.Printf("p50 Latency:    %.2f ms\n", p50)
		fmt.Printf("p95 Latency:    %.2f ms\n", p95)
		fmt.Printf("p99 Latency:    %.2f ms\n", p99)
	}
}
