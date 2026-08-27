#!/usr/bin/env python3
"""
Dummy Backend Server for Reverse Proxy Testing.

Provides endpoints that respond with metadata about the server instance (backend name,
requested path, HTTP method, timestamp, and echoed request body for POST/PUT requests).
Supports health checks on `/health`.
"""

import os
import sys
import logging
from datetime import datetime, timezone
from flask import Flask, request, jsonify

# Configure application logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s [%(levelname)s] [%(name)s] %(message)s',
    datefmt='%Y-%m-%dT%H:%M:%S%z',
    handlers=[logging.StreamHandler(sys.stdout)]
)
logger = logging.getLogger("backend-server")

app = Flask(__name__)

# Read configuration from environment variables
BACKEND_NAME = os.environ.get("BACKEND_NAME", "backend-unknown")
PORT = int(os.environ.get("PORT", "9001"))
HOST = os.environ.get("HOST", "0.0.0.0")


@app.before_request
def log_request_info():
    """Log details of incoming HTTP request."""
    logger.info(
        "Incoming request: %s %s from %s",
        request.method,
        request.full_path if request.query_string else request.path,
        request.remote_addr
    )


@app.route("/health", methods=["GET"])
def health():
    """Health check endpoint."""
    return jsonify({
        "status": "healthy",
        "backend": BACKEND_NAME,
        "timestamp": datetime.now(timezone.utc).isoformat()
    }), 200


@app.route("/", defaults={"path": ""}, methods=["GET", "POST", "PUT", "DELETE", "PATCH", "HEAD", "OPTIONS"])
@app.route("/<path:path>", methods=["GET", "POST", "PUT", "DELETE", "PATCH", "HEAD", "OPTIONS"])
def catch_all(path: str):
    """
    Catch-all route returning backend metadata and echoing request details.
    """
    normalized_path = "/" + path
    now_iso = datetime.now(timezone.utc).isoformat()

    response_payload = {
        "backend": BACKEND_NAME,
        "path": normalized_path,
        "method": request.method,
        "timestamp": now_iso
    }

    # Handle body echoing for methods that typically carry a payload
    if request.method in ["POST", "PUT", "PATCH"]:
        if request.is_json:
            try:
                response_payload["body"] = request.get_json(silent=True)
            except Exception:
                response_payload["body"] = request.get_data(as_text=True)
        else:
            # Fall back to raw text if not JSON or if JSON parsing fails
            raw_text = request.get_data(as_text=True)
            response_payload["body"] = raw_text

    logger.info("Processed %s for %s - Backend: %s", request.method, normalized_path, BACKEND_NAME)
    return jsonify(response_payload), 200


if __name__ == "__main__":
    logger.info("Starting %s on %s:%d", BACKEND_NAME, HOST, PORT)
    app.run(host=HOST, port=PORT)
