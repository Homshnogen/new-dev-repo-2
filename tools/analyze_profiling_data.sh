#!/bin/bash

# Check if the directory is provided and exists
PROFILING_DIR=$1
if [ -z "$PROFILING_DIR" ] || [ ! -d "$PROFILING_DIR" ]; then
    echo "Usage: $0 <profiling data directory>"
    exit 1
fi

echo "Profiling Analysis Report"
echo "-------------------------"

# Loop through all .callgrind files in the specified directory
for file in "$PROFILING_DIR"/*.callgrind; do
    if [ -f "$file" ]; then
        echo "Analyzing $(basename "$file")..."
        # Extract the total number of instructions executed
        total_instructions=$(grep -m 1 "summary:" "$file" | awk '{print $2}')
        echo "Total Instructions Executed: $total_instructions"
        echo ""
    fi
done
