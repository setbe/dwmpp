#!/usr/bin/env bash
set -euo pipefail

if ! command -v Xephyr >/dev/null 2>&1; then
  echo "Xephyr not found" >&2
  exit 1
fi
if ! command -v xdpyinfo >/dev/null 2>&1; then
  echo "xdpyinfo not found" >&2
  exit 1
fi
if ! command -v st >/dev/null 2>&1; then
  echo "st not found" >&2
  exit 1
fi
if ! command -v ps >/dev/null 2>&1; then
  echo "ps not found" >&2
  exit 1
fi

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
WM_BIN="${ROOT_DIR}/dwm"
RESERVED_DISPLAYS=""

if [ ! -x "${WM_BIN}" ]; then
  echo "Build port binary as ${WM_BIN} first" >&2
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

cpu_mem_line() {
  local pid="$1"
  local label="$2"
  if ! kill -0 "${pid}" 2>/dev/null; then
    printf "%s(pid=%s): exited" "${label}" "${pid}"
    return
  fi

  local stats cpu rss_kb rss_mb
  stats="$(ps -p "${pid}" -o %cpu=,rss= 2>/dev/null | awk 'NF{print $1" "$2; exit}')"
  if [ -z "${stats}" ]; then
    printf "%s(pid=%s): unavailable" "${label}" "${pid}"
    return
  fi

  cpu="$(printf '%s' "${stats}" | awk '{print $1}')"
  rss_kb="$(printf '%s' "${stats}" | awk '{print $2}')"
  rss_mb="$(awk -v kb="${rss_kb}" 'BEGIN { printf "%.1f", kb/1024 }')"
  printf "%s(pid=%s): CPU=%s%% RAM=%sMB" "${label}" "${pid}" "${cpu}" "${rss_mb}"
}

cleanup() {
  [ -n "${st1_pid:-}" ] && kill "${st1_pid}" 2>/dev/null || true
  [ -n "${st2_pid:-}" ] && kill "${st2_pid}" 2>/dev/null || true
  [ -n "${wm_pid:-}" ] && kill "${wm_pid}" 2>/dev/null || true
  [ -n "${xephyr_pid:-}" ] && kill "${xephyr_pid}" 2>/dev/null || true
  [ -n "${st1_pid:-}" ] && wait "${st1_pid}" 2>/dev/null || true
  [ -n "${st2_pid:-}" ] && wait "${st2_pid}" 2>/dev/null || true
  [ -n "${wm_pid:-}" ] && wait "${wm_pid}" 2>/dev/null || true
  [ -n "${xephyr_pid:-}" ] && wait "${xephyr_pid}" 2>/dev/null || true
}

trap cleanup EXIT INT TERM

display_num="$(pick_free_display)" || {
  echo "No free DISPLAY found in range :90..:120" >&2
  exit 1
}
display_name=":${display_num}"

Xephyr "${display_name}" -ac -screen 1280x800 >/tmp/xephyr_${display_num}.log 2>&1 &
xephyr_pid=$!

if ! wait_for_display "${display_name}"; then
  echo "Xephyr did not become ready on ${display_name}" >&2
  exit 1
fi

DISPLAY="${display_name}" "${WM_BIN}" >/tmp/dwm_manual_wait.log 2>&1 &
wm_pid=$!

sleep 1
DISPLAY="${display_name}" st >/tmp/st_${display_num}_1.log 2>&1 &
st1_pid=$!
DISPLAY="${display_name}" st >/tmp/st_${display_num}_2.log 2>&1 &
st2_pid=$!

echo "Manual Xephyr session started on ${display_name}."
echo "Window-manager PID: ${wm_pid}; Xephyr PID: ${xephyr_pid}."
echo "Waiting 60 seconds for manual GUI testing..."

for elapsed in $(seq 0 3 60); do
  ts="$(date +%H:%M:%S)"
  wm_line="$(cpu_mem_line "${wm_pid}" "dwm")"
  xephyr_line="$(cpu_mem_line "${xephyr_pid}" "Xephyr")"
  printf "[%s +%02ss] %s | %s\n" "${ts}" "${elapsed}" "${wm_line}" "${xephyr_line}"

  if [ "${elapsed}" -lt 60 ]; then
    sleep 3
  fi
done

echo "Manual wait complete. Shutting down." 
