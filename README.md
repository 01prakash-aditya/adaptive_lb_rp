# High-Performance Adaptive Reverse Proxy & Load Balancer

[![License: MIT](https://img.shields.io/github/license/01prakash-aditya/adaptive_lb_rp)](https://github.com/01prakash-aditya/adaptive_lb_rp/blob/main/LICENSE)

A systems-level, high-performance reverse proxy and load balancer written from scratch in **C++20**. Designed for high throughput and low latency, this proxy utilizes Linux's `epoll` for non-blocking, event-driven I/O and features a custom-built HTTP/1.1 parser alongside advanced traffic management capabilities.

## Architecture

```mermaid
graph LR
    Client([Client]) -->|HTTP Request :8080| Proxy

    subgraph High-Performance Proxy Core [C++20 / Linux]
        direction TB
        Epoll[Epoll Event Loop]
        RateLimiter[Token Bucket Rate Limiter]
        Parser[Custom HTTP/1.1 Parser]
        LB[Load Balancer]
        Health[Active Health Checker]
        Forwarder[TCP Forwarder & Conn Pool]
        Logger[Structured JSON Logger]

        Epoll --> RateLimiter
        RateLimiter -->|Pass| Parser
        Parser --> LB
        LB --> Forwarder
        Forwarder --> Logger
        Health -.->|Pings| LB
    end

    Forwarder -->|Keep-Alive| B1(Backend 1)
    Forwarder -->|Keep-Alive| B2(Backend 2)
    Forwarder -->|Keep-Alive| B3(Backend 3)
```

## Core Features

* **Non-Blocking TCP Core**: Utilizes Linux `epoll` with edge-triggered notifications for highly efficient, asynchronous connection handling.
* **Custom HTTP/1.1 Parser**: Incremental parsing supporting `Content-Length` and chunked `Transfer-Encoding` without relying on external HTTP libraries.
* **Multi-Backend Load Balancing**: Pluggable strategies to route traffic:
  * **Round Robin**: Evenly cycles requests.
  * **Weighted Round Robin**: Adjusts traffic flow based on server capacity.
  * **Least Connections**: Dynamically routes to the least busy server using atomic trackers.
  * **IP Hash**: Ensures session stickiness by hashing the client's IP.
* **Health Checking & Auto-Failover**:
  * *Active Probing*: A background thread continually verifies backend availability.
  * *Passive Monitoring*: Detects timeouts or refused connections in real-time.
  * *Zero-Downtime Failover*: Unhealthy nodes are instantly removed from the routing pool.
* **Traffic Control (Rate Limiting)**: Thread-safe Token Bucket implementation offering both **Global** throughput caps and strict **Per-IP** request limits (returns `429 Too Many Requests`).
* **Connection Pooling**: Drastically reduces latency by caching and reusing idle Keep-Alive TCP sockets instead of initiating a 3-way handshake on every request.
* **Observability**: Structured JSON logging recording latency, HTTP methods, and status codes. Injects `X-Request-ID` and `X-Forwarded-For`.

## Technology Stack

| Component | Technology |
| :--- | :--- |
| **Proxy Core** | C++20 (POSIX sockets, `epoll`) |
| **Compiler / Build** | GCC 12+, CMake (3.20+), Ninja |
| **Containerization** | Docker, Docker Compose |
| **Testing** | Python 3 (Requests, concurrent.futures) |
| **Dummy Backends** | Python 3.11, Flask |

## Getting Started

The entire environment (proxy + 3 backend instances) is containerized for easy testing.

### 1. Start the Environment
Spin up the reverse proxy and the backend instances on a shared Docker network:
```bash
docker compose -f docker/docker-compose.yml up --build -d
```

### 2. Verify Traffic Routing (Distribution Test)
The proxy listens on `localhost:8080`. You can automatically test the load balancing distribution using the provided Python script:
```bash
python scripts/test_distribution.py http://localhost:8080/
```
*(Expected: Output demonstrating traffic evenly split ~33% across all three backend containers).*

### 3. Test Failover & Recovery
Monitor how the proxy handles a backend going down in real-time:
```bash
python scripts/test_failover.py http://localhost:8080/
```
While the script is running, open a new terminal and stop a backend (e.g., `docker stop btp-backend-2-1`). The script will log the failover recovery time.

### 4. Test Rate Limiting
Simulate a concurrent traffic burst to trigger the Token Bucket rate limiter:
```bash
python scripts/test_rate_limit.py http://localhost:8080/
```
*(Expected: A mix of `200 OK` and `429 Too Many Requests` depending on the burst size).*

### 5. Inspect Observability Logs
The proxy emits structured JSON logs. View them with:
```bash
docker compose -f docker/docker-compose.yml logs --tail 50 proxy
```

## Further ideas:

- [ ] **In-Memory Caching**: LRU Cache layer to intercept repeated identical requests.
- [ ] Cache invalidation and TTL tracking.
- [ ] Adaptive Load Balancing: Real-time feedback loops based on latency metrics.

## License

This project is licensed under the MIT License.
