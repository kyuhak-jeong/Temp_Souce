#!/bin/sh

# Config
LOG_FILE="/mnt/udisk/safeview_debug.log"
TEST_DURATION=$((96 * 3600)) # 96 hours in seconds
START_TIME=$(date +%s)
END_TIME=$((START_TIME + TEST_DURATION))
RESTART_COUNT=0

echo "=== 24-HOUR DEBUGGING START: $(date) ===" > "$LOG_FILE"
echo "Start Epoch: $START_TIME, Target End Epoch: $END_TIME" >> "$LOG_FILE"

while true; do
    CURRENT_TIME=$(date +%s)
    if [ "$CURRENT_TIME" -ge "$END_TIME" ]; then
        echo "=== 24-HOUR TEST COMPLETED SUCCESSFULLY: $(date) ===" >> "$LOG_FILE"
        break
    fi
    
    REMAINING_TIME=$((END_TIME - CURRENT_TIME))
    echo "--- Launching SafeView under GDB (Restart Count: $RESTART_COUNT, Remaining Time: ${REMAINING_TIME}s) at $(date) ---" >> "$LOG_FILE"
    
    # Run under GDB in batch mode
    gdb -batch -ex run -ex bt ./SafeView_R2.1.0H9 < /dev/null >> "$LOG_FILE" 2>&1
    GDB_EXIT_CODE=$?
    
    echo "--- SafeView under GDB exited with code $GDB_EXIT_CODE at $(date) ---" >> "$LOG_FILE"
    
    RESTART_COUNT=$((RESTART_COUNT + 1))
    
    # Sleep a bit before restarting to prevent rapid loop if it crashes instantly
    sleep 5
done
