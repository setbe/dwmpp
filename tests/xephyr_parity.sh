#!/usr/bin/env bash
set -euo pipefail

if ! command -v Xephyr >/dev/null 2>&1; then
  echo "Xephyr not found" >&2
  exit 1
fi
if ! command -v xdotool >/dev/null 2>&1; then
  echo "xdotool not found" >&2
  exit 1
fi
if ! command -v xdpyinfo >/dev/null 2>&1; then
  echo "xdpyinfo not found" >&2
  exit 1
fi

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
UPSTREAM_BIN="${ROOT_DIR}/dwm_upstream"
PORT_BIN="${ROOT_DIR}/dwm"
RESERVED_DISPLAYS=""

wait_for_display() {
  local display_name="$1"
  local retries=50

  while [ "${retries}" -gt 0 ]; do
    if DISPLAY="${display_name}" xdpyinfo >/dev/null 2>&1; then
      return 0
    fi
    retries=$((retries - 1))
    sleep 0.1
  done
  return 1
}

reserve_display() {
  local n="$1"
  case " ${RESERVED_DISPLAYS} " in
    *" ${n} "*) return 1 ;;
  esac
  RESERVED_DISPLAYS="${RESERVED_DISPLAYS} ${n}"
  return 0
}

pick_free_display() {
  local n
  for n in $(seq 90 120); do
    if [ -e "/tmp/.X${n}-lock" ]; then
      continue
    fi
    if ! reserve_display "${n}"; then
      continue
    fi
    echo "${n}"
    return 0
  done
  return 1
}

normalize_log() {
  sed -E 's/window id # 0x[0-9a-fA-F]+/window id # <XID>/g'
}

client_count() {
  local display_name="$1"
  DISPLAY="${display_name}" xprop -root _NET_CLIENT_LIST 2>/dev/null \
    | grep -Eo '0x[0-9a-fA-F]+' \
    | wc -l
}

run_case() {
  local wm_bin="$1"
  local display_num="$2"
  local log="$3"
  local mod_key="$4"
  local display_name=":${display_num}"
  local xephyr_log="/tmp/xephyr_${display_num}.log"
  local key_delay_s="0.2"

  Xephyr "${display_name}" -ac -screen 1280x800 >"${xephyr_log}" 2>&1 &
  local xephyr_pid=$!

  if ! wait_for_display "${display_name}"; then
    echo "Xephyr did not become ready on ${display_name}" >>"${log}"
    cat "${xephyr_log}" >>"${log}" 2>/dev/null || true
    kill "${xephyr_pid}" 2>/dev/null || true
    wait "${xephyr_pid}" 2>/dev/null || true
    return 1
  fi

  DISPLAY="${display_name}" "${wm_bin}" >"${log}" 2>&1 &
  local wm_pid=$!
  sleep 1

  DISPLAY="${display_name}" st >/tmp/st_${display_num}_1.log 2>&1 &
  DISPLAY="${display_name}" st >/tmp/st_${display_num}_2.log 2>&1 &
  sleep 1
  local count_before_kill
  count_before_kill="$(client_count "${display_name}")"

  DISPLAY="${display_name}" xdotool key "${mod_key}+j"
  sleep "${key_delay_s}"
  DISPLAY="${display_name}" xdotool key "${mod_key}+k"
  sleep "${key_delay_s}"
  DISPLAY="${display_name}" xdotool key "${mod_key}+Shift+c"
  local expected_after_kill
  expected_after_kill=$((count_before_kill - 1))
  if [ "${expected_after_kill}" -lt 0 ]; then
    expected_after_kill=0
  fi
  local count_after_kill
  local tries
  for tries in $(seq 1 15); do
    count_after_kill="$(client_count "${display_name}")"
    if [ "${count_after_kill}" -le "${expected_after_kill}" ]; then
      break
    fi
    sleep 0.1
  done
  DISPLAY="${display_name}" xdotool key "${mod_key}+space"
  sleep "${key_delay_s}"
  DISPLAY="${display_name}" xdotool key "${mod_key}+1"
  sleep "${key_delay_s}"
  DISPLAY="${display_name}" xdotool key "${mod_key}+2"
  sleep "${key_delay_s}"
  DISPLAY="${display_name}" xdotool key "${mod_key}+Control+1"
  sleep "${key_delay_s}"
  DISPLAY="${display_name}" xdotool key "${mod_key}+t"
  sleep "${key_delay_s}"
  DISPLAY="${display_name}" xdotool key "${mod_key}+m"
  sleep "${key_delay_s}"
  DISPLAY="${display_name}" xdotool key "${mod_key}+f"
  sleep 1

  local prev_list=""
  local cur_list=""
  local i
  for i in 1 2 3 4 5; do
    cur_list="$(DISPLAY="${display_name}" xprop -root _NET_CLIENT_LIST 2>/dev/null || true)"
    if [ -n "${prev_list}" ] && [ "${cur_list}" = "${prev_list}" ]; then
      break
    fi
    prev_list="${cur_list}"
    sleep 0.2
  done

  DISPLAY="${display_name}" xprop -root _NET_ACTIVE_WINDOW _NET_CLIENT_LIST _NET_CURRENT_DESKTOP >>"${log}" 2>&1 || true

  kill "${wm_pid}" 2>/dev/null || true
  kill "${xephyr_pid}" 2>/dev/null || true
  wait "${wm_pid}" 2>/dev/null || true
  wait "${xephyr_pid}" 2>/dev/null || true
}

if [ ! -x "${UPSTREAM_BIN}" ]; then
  echo "Build upstream binary as ${UPSTREAM_BIN} first" >&2
  exit 1
fi
if [ ! -x "${PORT_BIN}" ]; then
  echo "Build port binary as ${PORT_BIN} first" >&2
  exit 1
fi
if [ -z "${DISPLAY:-}" ]; then
  echo "DISPLAY is not set. Run this test from an active graphical session." >&2
  exit 1
fi
if ! xdpyinfo >/dev/null 2>&1; then
  echo "Host display is unavailable for nested Xephyr. Open a graphical session (X11 or Wayland+XWayland) and rerun." >&2
  exit 1
fi

rm -f /tmp/dwm_upstream_parity.log /tmp/dwm_port_parity.log
display_upstream="$(pick_free_display)" || {
  echo "No free DISPLAY found in range :90..:120" >&2
  exit 1
}
display_port="$(pick_free_display)" || {
  echo "Only one free DISPLAY found in range :90..:120" >&2
  exit 1
}

run_case "${UPSTREAM_BIN}" "${display_upstream}" /tmp/dwm_upstream_parity.log Alt
run_case "${PORT_BIN}" "${display_port}" /tmp/dwm_port_parity.log Super

normalize_log < /tmp/dwm_upstream_parity.log > /tmp/dwm_upstream_parity.norm.log
normalize_log < /tmp/dwm_port_parity.log > /tmp/dwm_port_parity.norm.log

echo "==== DIFF (upstream vs port, normalized) ===="
diff -u /tmp/dwm_upstream_parity.norm.log /tmp/dwm_port_parity.norm.log || true
