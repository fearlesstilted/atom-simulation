#!/usr/bin/env bash

set -u

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BUILD="$ROOT/build"
STAMP=$(mktemp)
APP_PID=

stop_app() {
    [[ -n "$APP_PID" ]] || return
    kill -0 "$APP_PID" 2>/dev/null || return

    window=$(xdotool search --pid "$APP_PID" \
        --name "Atomic Orbital Lab" 2>/dev/null | head -1)
    if [[ -n "$window" ]]; then
        xdotool windowactivate --sync "$window" 2>/dev/null || true
        xdotool key Escape 2>/dev/null || true
    fi

    for _ in {1..20}; do
        kill -0 "$APP_PID" 2>/dev/null || break
        sleep 0.05
    done
    if kill -0 "$APP_PID" 2>/dev/null; then
        kill "$APP_PID"
    fi
    wait "$APP_PID" 2>/dev/null || true
    APP_PID=
}

start_app() {
    "$BUILD/atom" &
    APP_PID=$!
}

cleanup() {
    stop_app
    rm -f "$STAMP"
}
trap cleanup EXIT
trap 'exit 0' INT TERM

cmake -S "$ROOT" -B "$BUILD" || exit 1
cmake --build "$BUILD" || exit 1
ctest --test-dir "$BUILD" --output-on-failure || exit 1
touch "$STAMP"
start_app

printf 'Watching C++ sources. GLSL reloads live inside the running app.\n'

while sleep 0.4; do
    changed=$(find "$ROOT/src" "$ROOT/tests" "$ROOT/CMakeLists.txt" \
        -type f \( -name '*.cpp' -o -name '*.hpp' -o -name 'CMakeLists.txt' \) \
        -newer "$STAMP" -print -quit)
    [[ -n "$changed" ]] || continue
    touch "$STAMP"

    printf '\nChange detected: %s\n' "$changed"
    if cmake -S "$ROOT" -B "$BUILD" \
        && cmake --build "$BUILD" \
        && ctest --test-dir "$BUILD" --output-on-failure; then
        stop_app
        start_app
        printf 'Reloaded C++ application state.\n'
    else
        printf 'Build or tests failed; keeping the previous process.\n'
    fi
done
