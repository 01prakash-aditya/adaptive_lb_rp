# Week 5 & 6 Tasks

## 1. LRU Caching (Week 5)
- [x] Create `src/cache/lru_cache.hpp`
- [x] Create `src/cache/lru_cache.cpp`
- [x] Integrate into `src/server/server.cpp` (Intercept GET requests, fetch from cache, update on miss)

## 2. Observability & Metrics (Week 6)
- [x] Create `src/metrics/registry.hpp` and `.cpp`
- [x] Update `src/server/server.cpp` to expose `/metrics` route
- [x] Update `CMakeLists.txt`

## 3. Infrastructure & Dashboards (Week 6)
- [x] Create `docker/prometheus.yml`
- [x] Create Grafana provisioning files (`docker/grafana/provisioning/datasources/prometheus.yml`, `docker/grafana/provisioning/dashboards/dashboard.yml`, `docker/grafana/dashboards/proxy.json`)
- [x] Update `docker-compose.yml` with prometheus and grafana services

## 4. Go Load Tester (Week 6)
- [x] Create `load_tester/main.go`
- [x] Create `load_tester/Dockerfile`
- [x] Add to `docker-compose.yml` or ensure it runs smoothly
