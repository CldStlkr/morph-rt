#!/bin/bash

# Configuration
PROJECT_ROOT="/home/neel/Dev/personal/morph-rt"
BUILD_DIR="$PROJECT_ROOT/build"
BINARY="$BUILD_DIR/traffic_stop_ffi"
CHIP="STM32F407VG"
WAIT_TIME=10
CLOCK_MHZ=168

# Parse arguments
while [[ "$#" -gt 0 ]]; do
  case $1 in
  --wait)
    WAIT_TIME="$2"
    shift
    ;;
  --stress)
    STRESS_N="$2"
    BINARY="$BUILD_DIR/stress_n$STRESS_N"
    shift
    ;;
  *)
    echo "Unknown parameter: $1"
    exit 1
    ;;
  esac
  shift
done

if [[ -n "$STRESS_N" ]]; then
  OUT_DIR="/tmp/N$STRESS_N"
else
  OUT_DIR="/tmp/$(basename "$BINARY")"
fi

mkdir -p "$OUT_DIR"

[ -n "$STRESS_N" ] && echo "=== Stress Test: N=$STRESS_N sleeping tasks ==="

echo "--- 1. Building Project ---"
make -C "$BUILD_DIR" -j$(nproc) || exit 1

echo "--- 2. Flashing and Resetting ---"
probe-rs download --chip "$CHIP" --binary-format bin --base-address 0x08000000 "$BINARY.bin" || exit 1
probe-rs reset --chip "$CHIP" || exit 1

echo "--- 3. Collecting Data (Waiting $WAIT_TIME seconds) ---"
sleep "$WAIT_TIME"

echo "--- 4. Dumping RAM via J-Link ---"

# Lookup a symbol address from the ELF
nm_addr() { arm-none-eabi-nm "$BINARY" 2>/dev/null | grep " $1\$" | awk '{print $1}'; }

# Ring buffers
SYSTICK_ADDR=$(nm_addr "systick_periods")
CTXSW_ADDR=$(nm_addr "ctxsw_durations")
TICK_DUR_ADDR=$(nm_addr "tick_durations")
DRAIN_ADDR=$(nm_addr "unlock_drain_durations")
LATENCY_ADDR=$(nm_addr "irq_to_task_latencies")

# Scalar globals
SYSTICK_MIN_ADDR=$(nm_addr "systick_period_min")
SYSTICK_MAX_ADDR=$(nm_addr "systick_period_max")
CTXSW_MIN_ADDR=$(nm_addr "ctxsw_duration_min")
CTXSW_MAX_ADDR=$(nm_addr "ctxsw_duration_max")
TICK_MIN_ADDR=$(nm_addr "tick_duration_min")
TICK_MAX_ADDR=$(nm_addr "tick_duration_max")
DRAIN_MIN_ADDR=$(nm_addr "unlock_drain_min")
DRAIN_MAX_ADDR=$(nm_addr "unlock_drain_max")
DRAIN_IDX_ADDR=$(nm_addr "unlock_drain_idx")
LATENCY_MIN_ADDR=$(nm_addr "irq_to_task_min")
LATENCY_MAX_ADDR=$(nm_addr "irq_to_task_max")
LATENCY_COUNT_ADDR=$(nm_addr "latency_sample_count")

if [ -z "$SYSTICK_ADDR" ] || [ -z "$CTXSW_ADDR" ] || [ -z "$TICK_DUR_ADDR" ]; then
  echo "Error: Could not find core buffer addresses in $BINARY"
  exit 1
fi

echo "Found systick_periods          at 0x$SYSTICK_ADDR"
echo "Found ctxsw_durations          at 0x$CTXSW_ADDR"
echo "Found tick_durations           at 0x$TICK_DUR_ADDR"
[ -n "$DRAIN_ADDR" ] && echo "Found unlock_drain_durations   at 0x$DRAIN_ADDR"
[ -n "$LATENCY_ADDR" ] && echo "Found irq_to_task_latencies    at 0x$LATENCY_ADDR"

# Emit a savebin command only when addr is non-empty
savebin_if() {
  local addr="$1" file="$2" size="${3:-4}"
  [ -n "$addr" ] && echo "savebin $file 0x$addr $size"
}

# Build and execute a single JLink session for all reads
{
  echo "h"
  # 1 KB ring buffers (1024 x uint32_t)
  savebin_if "$SYSTICK_ADDR" $OUT_DIR/systick_raw.bin 0x1000
  savebin_if "$CTXSW_ADDR" $OUT_DIR/ctxsw_raw.bin 0x1000
  savebin_if "$TICK_DUR_ADDR" $OUT_DIR/tick_dur_raw.bin 0x1000
  savebin_if "$DRAIN_ADDR" $OUT_DIR/drain_raw.bin 0x1000
  savebin_if "$LATENCY_ADDR" $OUT_DIR/latency_raw.bin 0x1000
  # Scalars (4 bytes each)
  savebin_if "$SYSTICK_MIN_ADDR" $OUT_DIR/s_pmin.bin
  savebin_if "$SYSTICK_MAX_ADDR" $OUT_DIR/s_pmax.bin
  savebin_if "$CTXSW_MIN_ADDR" $OUT_DIR/s_cmin.bin
  savebin_if "$CTXSW_MAX_ADDR" $OUT_DIR/s_cmax.bin
  savebin_if "$TICK_MIN_ADDR" $OUT_DIR/s_tmin.bin
  savebin_if "$TICK_MAX_ADDR" $OUT_DIR/s_tmax.bin
  savebin_if "$DRAIN_MIN_ADDR" $OUT_DIR/s_dmin.bin
  savebin_if "$DRAIN_MAX_ADDR" $OUT_DIR/s_dmax.bin
  savebin_if "$DRAIN_IDX_ADDR" $OUT_DIR/s_didx.bin
  savebin_if "$LATENCY_MIN_ADDR" $OUT_DIR/s_lmin.bin
  savebin_if "$LATENCY_MAX_ADDR" $OUT_DIR/s_lmax.bin
  savebin_if "$LATENCY_COUNT_ADDR" $OUT_DIR/s_lcnt.bin
  echo "q"
} | JLinkExe -device "$CHIP" -if SWD -speed 4000 -autoconnect 1 >/dev/null

# Read a 4-byte binary file as unsigned decimal
read_scalar() {
  [ -f "$1" ] && od -An -tu4 -N4 "$1" | tr -d ' \n' || echo ""
}

SYSTICK_MIN_VAL=$(read_scalar $OUT_DIR/s_pmin.bin)
SYSTICK_MAX_VAL=$(read_scalar $OUT_DIR/s_pmax.bin)
CTXSW_MIN_VAL=$(read_scalar $OUT_DIR/s_cmin.bin)
CTXSW_MAX_VAL=$(read_scalar $OUT_DIR/s_cmax.bin)
TICK_MIN_VAL=$(read_scalar $OUT_DIR/s_tmin.bin)
TICK_MAX_VAL=$(read_scalar $OUT_DIR/s_tmax.bin)
DRAIN_MIN_VAL=$(read_scalar $OUT_DIR/s_dmin.bin)
DRAIN_MAX_VAL=$(read_scalar $OUT_DIR/s_dmax.bin)
DRAIN_IDX_VAL=$(read_scalar $OUT_DIR/s_didx.bin)
LATENCY_MIN_VAL=$(read_scalar $OUT_DIR/s_lmin.bin)
LATENCY_MAX_VAL=$(read_scalar $OUT_DIR/s_lmax.bin)
LATENCY_COUNT_VAL=$(read_scalar $OUT_DIR/s_lcnt.bin)

# Convert cycle count to microseconds
cyc_to_us() {
  local cyc="$1"
  if [[ "$cyc" =~ ^[0-9]+$ ]]; then
    awk "BEGIN { printf \"%.2f\", $cyc / $CLOCK_MHZ }"
  else
    echo "N/A"
  fi
}

echo ""
echo "--- 5. Jitter Results (SysTick Heartbeat) ---"
echo "Target: $((CLOCK_MHZ * 1000)) cycles (1.000ms at ${CLOCK_MHZ}MHz)"
od -An -td4 -w4 $OUT_DIR/systick_raw.bin | head -n 20 | awk -v mhz="$CLOCK_MHZ" '
{
    if ($1 == "*") { print "  * (repeated)"; next }
    printf "  %s cycles (~%.3f ms / %.2f us jitter)\n", $1, $1/(mhz*1000.0), ($1-(mhz*1000))/(mhz*1.0)
}'
echo "  Min: $SYSTICK_MIN_VAL cyc (~$(cyc_to_us $SYSTICK_MIN_VAL) us)  Max: $SYSTICK_MAX_VAL cyc (~$(cyc_to_us $SYSTICK_MAX_VAL) us)"

echo ""
echo "--- 6. Context Switch Overhead ---"
echo "Raw cycle counts for kernel task swaps:"
od -An -td4 -w4 $OUT_DIR/ctxsw_raw.bin | head -n 20 | awk -v mhz="$CLOCK_MHZ" '
{
    if ($1 == "*") { print "  * (repeated)"; next }
    printf "  %s cycles (~%.2f us overhead)\n", $1, $1/mhz
}'
echo "  Min: $CTXSW_MIN_VAL cyc (~$(cyc_to_us $CTXSW_MIN_VAL) us)  Max: $CTXSW_MAX_VAL cyc (~$(cyc_to_us $CTXSW_MAX_VAL) us)"

echo ""
echo "--- 7. Tick Processing Duration ---"
echo "Execution time of scheduler_tick() (O(N) delay list scan):"
od -An -td4 -w4 $OUT_DIR/tick_dur_raw.bin | awk -v mhz="$CLOCK_MHZ" '
BEGIN { max=0; sum=0; count=0 }
{
    if ($1 == "*") next
    val = $1
    if (val > 0 && val < 100000) {
        count++; sum += val
        if (val > max) max = val
        dist[val]++
    }
}
END {
    if (count == 0) { print "  No data collected."; exit }
    for (v in dist) if (dist[v] > max_count) { max_count = dist[v]; typical = v }
    printf "  Typical Duration: %d cycles (~%.2f us)\n", typical, typical/mhz
    printf "  Max Duration:     %d cycles (~%.2f us)\n", max, max/mhz
    print "\n  Distribution (Cycles):"
    cmd = "sort -n"
    for (v in dist) printf "    %4d cycles: %d occurrences\n", v, dist[v] | cmd
    close(cmd)
}'
echo "  Min: $TICK_MIN_VAL cyc (~$(cyc_to_us $TICK_MIN_VAL) us)  Max: $TICK_MAX_VAL cyc (~$(cyc_to_us $TICK_MAX_VAL) us)"

if [ -f $OUT_DIR/drain_raw.bin ]; then
  echo ""
  echo "--- 8. Unlock-Drain Duration ---"
  echo "Time spent draining pending ticks/tasks in scheduler_unlock() (samples: $DRAIN_IDX_VAL):"
  od -An -td4 -w4 $OUT_DIR/drain_raw.bin | awk -v mhz="$CLOCK_MHZ" '
  BEGIN { max=0; sum=0; count=0 }
  {
      if ($1 == "*") next
      val = $1
      if (val > 0 && val < 1000000) {
          count++; sum += val
          if (val > max) max = val
          dist[val]++
      }
  }
  END {
      if (count == 0) { print "  No drain events recorded."; exit }
      for (v in dist) if (dist[v] > max_count) { max_count = dist[v]; typical = v }
      printf "  Typical Duration: %d cycles (~%.2f us)\n", typical, typical/mhz
      printf "  Max Duration:     %d cycles (~%.2f us)\n", max, max/mhz
  }'
  echo "  Min: $DRAIN_MIN_VAL cyc (~$(cyc_to_us $DRAIN_MIN_VAL) us)  Max: $DRAIN_MAX_VAL cyc (~$(cyc_to_us $DRAIN_MAX_VAL) us)"
fi

if [ -f $OUT_DIR/latency_raw.bin ]; then
  echo ""
  echo "--- 9. IRQ-to-Task Latency ---"
  echo "Time from IRQ6 pending to latency_task first instruction (samples: $LATENCY_COUNT_VAL):"
  od -An -td4 -w4 $OUT_DIR/latency_raw.bin | awk -v mhz="$CLOCK_MHZ" '
  BEGIN { max=0; min=999999999; sum=0; count=0 }
  {
      if ($1 == "*") next
      val = $1
      if (val > 0 && val < 1000000) {
          count++; sum += val
          if (val > max) max = val
          if (val < min) min = val
          dist[val]++
      }
  }
  END {
      if (count == 0) { print "  No latency data recorded."; exit }
      for (v in dist) if (dist[v] > max_count) { max_count = dist[v]; typical = v }
      printf "  Typical Latency: %d cycles (~%.2f us)\n", typical, typical/mhz
      printf "  Min Latency:     %d cycles (~%.2f us)\n", min, min/mhz
      printf "  Max Latency:     %d cycles (~%.2f us)\n", max, max/mhz
      printf "  Avg Latency:     %.0f cycles (~%.2f us)\n", sum/count, sum/count/mhz
  }'
  if [[ "$LATENCY_COUNT_VAL" =~ ^[0-9]+$ ]] && [ "$LATENCY_COUNT_VAL" -gt 0 ]; then
    echo "  Min: $LATENCY_MIN_VAL cyc (~$(cyc_to_us $LATENCY_MIN_VAL) us)  Max: $LATENCY_MAX_VAL cyc (~$(cyc_to_us $LATENCY_MAX_VAL) us)"
  fi
fi

echo ""
echo "=== Summary (${CLOCK_MHZ}MHz clock, 1kHz tick) ==="
printf "  %-32s min=%-10s max=%s cycles\n" "SysTick period jitter:" "$SYSTICK_MIN_VAL" "$SYSTICK_MAX_VAL"
printf "  %-32s min=%-10s max=%s cycles\n" "Context switch overhead:" "$CTXSW_MIN_VAL" "$CTXSW_MAX_VAL"
printf "  %-32s min=%-10s max=%s cycles\n" "scheduler_tick() duration:" "$TICK_MIN_VAL" "$TICK_MAX_VAL"
[ -n "$DRAIN_MIN_ADDR" ] &&
  printf "  %-32s min=%-10s max=%s cycles  (n=%s)\n" "Unlock-drain duration:" "$DRAIN_MIN_VAL" "$DRAIN_MAX_VAL" "$DRAIN_IDX_VAL"
if [[ "$LATENCY_COUNT_VAL" =~ ^[0-9]+$ ]] && [ "$LATENCY_COUNT_VAL" -gt 0 ]; then
  printf "  %-32s min=%-10s max=%s cycles  (n=%s)\n" "IRQ-to-task latency:" "$LATENCY_MIN_VAL" "$LATENCY_MAX_VAL" "$LATENCY_COUNT_VAL"
fi

echo ""
echo "  In microseconds:"
printf "  %-32s min=%-10s max=%s us\n" "Context switch overhead:" "$(cyc_to_us $CTXSW_MIN_VAL)" "$(cyc_to_us $CTXSW_MAX_VAL)"
printf "  %-32s min=%-10s max=%s us\n" "scheduler_tick() duration:" "$(cyc_to_us $TICK_MIN_VAL)" "$(cyc_to_us $TICK_MAX_VAL)"
[ -n "$DRAIN_MIN_ADDR" ] &&
  printf "  %-32s min=%-10s max=%s us\n" "Unlock-drain duration:" "$(cyc_to_us $DRAIN_MIN_VAL)" "$(cyc_to_us $DRAIN_MAX_VAL)"
if [[ "$LATENCY_COUNT_VAL" =~ ^[0-9]+$ ]] && [ "$LATENCY_COUNT_VAL" -gt 0 ]; then
  printf "  %-32s min=%-10s max=%s us\n" "IRQ-to-task latency:" "$(cyc_to_us $LATENCY_MIN_VAL)" "$(cyc_to_us $LATENCY_MAX_VAL)"
fi

echo ""
echo "Analysis Complete. Raw files saved to $OUT_DIR/{systick,ctxsw,tick_dur,drain,latency}_raw.bin"
