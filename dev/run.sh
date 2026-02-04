#!/bin/bash
set -e

cd "$(dirname "$0")/.."

find_cmd() {
    local cmd="$1"
    local brew_prefix

    if command -v "$cmd" >/dev/null 2>&1; then
        command -v "$cmd"
        return 0
    fi

    case "$cmd" in
        fcgiwrap)
            for candidate in \
                /opt/homebrew/sbin/fcgiwrap \
                /opt/homebrew/opt/fcgiwrap/sbin/fcgiwrap \
                /usr/local/sbin/fcgiwrap \
                /usr/local/opt/fcgiwrap/sbin/fcgiwrap
            do
                if [ -x "$candidate" ]; then
                    echo "$candidate"
                    return 0
                fi
            done
            ;;
    esac

    if command -v brew >/dev/null 2>&1; then
        brew_prefix="$(brew --prefix "$cmd" 2>/dev/null || true)"
        if [ -n "$brew_prefix" ]; then
            for candidate in "$brew_prefix/bin/$cmd" "$brew_prefix/sbin/$cmd"; do
                if [ -x "$candidate" ]; then
                    echo "$candidate"
                    return 0
                fi
            done
        fi
    fi

    return 1
}

# Check dependencies
FCGIWRAP_BIN="$(find_cmd fcgiwrap || true)"
if [ -z "$FCGIWRAP_BIN" ]; then
    echo "fcgiwrap not found"
    exit 1
fi

NGINX_BIN="$(find_cmd nginx || true)"
if [ -z "$NGINX_BIN" ]; then
    echo "nginx not found"
    exit 1
fi

# Find nginx conf path from nix store
if command -v realpath >/dev/null 2>&1; then
    NGINX_BIN_REALPATH="$(realpath "$NGINX_BIN")"
else
    NGINX_BIN_REALPATH="$NGINX_BIN"
fi
NGINX_CONF_PATH="$(dirname "$(dirname "$NGINX_BIN_REALPATH")")/conf"

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
"$FCGIWRAP_BIN" -s unix:/tmp/fcgiwrap.socket &
sleep 1
chmod 666 /tmp/fcgiwrap.socket

# Start nginx
echo "Starting nginx on :80..."
echo "Open http://localhost"
sudo "$NGINX_BIN" -c /tmp/cgit-nginx.conf
