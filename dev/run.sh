#!/bin/bash
set -e

cd "$(dirname "$0")/.."

# Check dependencies
if ! command -v fcgiwrap &>/dev/null; then
    echo "fcgiwrap not found"
    exit 1
fi

if ! command -v nginx &>/dev/null; then
    echo "nginx not found"
    exit 1
fi

# Find nginx conf path from nix store
NGINX_CONF_PATH="$(dirname $(dirname $(readlink -f $(which nginx))))/conf"

# Generate nginx config with correct paths
sed "s|NGINX_CONF_PATH|$NGINX_CONF_PATH|g" dev/nginx.conf > /tmp/cgit-nginx.conf

# Cleanup on exit
cleanup() {
    echo "Stopping..."
    pkill -f "fcgiwrap.*cgit" 2>/dev/null || true
    pkill -f "nginx.*cgit-nginx" 2>/dev/null || true
    rm -f /tmp/fcgiwrap.socket /tmp/cgit-nginx.conf
}
trap cleanup EXIT

# Start fcgiwrap
echo "Starting fcgiwrap..."
fcgiwrap -s unix:/tmp/fcgiwrap.socket &
sleep 1
chmod 666 /tmp/fcgiwrap.socket

# Start nginx
echo "Starting nginx on :80..."
echo "Open http://localhost"
sudo nginx -c /tmp/cgit-nginx.conf
