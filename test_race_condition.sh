#!/bin/bash

# Clean up function
cleanup() {
  echo "Cleaning up processes..."
  pkill -f "./server_race"
  pkill -f "./server_fixed"
  exit 0
}

# Register the cleanup function for when script is terminated
trap cleanup SIGINT SIGTERM

# Function to test a server
test_server() {
  local server_type=$1
  local server_executable="./server_$server_type"
  
  # Clean up previous logs
  > "server_${server_type}.log"
  > "clients.log"
  
  echo "====================================="
  echo "Testing $server_type server"
  echo "====================================="
  
  # Build the server with the specified port
  echo "Compiling $server_type server..."
  gcc -o "$server_executable" "server_${server_type}.c" -pthread
  
  if [ ! -f "$server_executable" ]; then
    echo "ERROR: Failed to compile $server_executable"
    return 1
  fi
  
  # Get the port from the server type
  if [ "$server_type" == "race" ]; then
    local port=8080
  else
    local port=8081
  fi
  
  # Build the client with the specified port
  echo "Compiling client for port $port..."
  gcc -o client client.c -pthread -DPORT=$port
  
  if [ ! -f "client" ]; then
    echo "ERROR: Failed to compile client"
    return 1
  fi
  
  # Start the server
  echo "Starting $server_type server on port $port..."
  $server_executable &
  SERVER_PID=$!
  
  # Wait for server to initialize
  sleep 2
  
  # Check if server is running
  if ! ps -p $SERVER_PID > /dev/null; then
    echo "ERROR: $server_type server failed to start"
    return 1
  fi
  
  echo "Server PID: $SERVER_PID"
  echo "Running clients..."
  
  # Run clients
  ./run_clients
  
  # Wait for clients to complete
  echo "Waiting for clients to finish..."
  sleep 8
  
  # Kill the server
  echo "Stopping $server_type server..."
  kill $SERVER_PID 2>/dev/null || true
  wait $SERVER_PID 2>/dev/null || true
  
  # Display results summary
  echo "====================================="
  echo "Results for $server_type server:"
  echo "====================================="
  
  # Check for race conditions in the log
  if [ -f "server_${server_type}.log" ]; then
    echo "Analyzing server_${server_type}.log for race conditions..."
    
    # Count processed requests
    local NUM_PROCESSED=$(grep -c "\[PROCESSED\]" "server_${server_type}.log" 2>/dev/null || echo 0)
    if [ "$NUM_PROCESSED" -eq 0 ]; then
      NUM_PROCESSED=$(grep -c "\[PROCESSED" "server_${server_type}.log" 2>/dev/null || echo 0)
    fi
    
    echo "Number of processed requests: $NUM_PROCESSED"
    
    if [ "$NUM_PROCESSED" -gt 0 ]; then
      # Extract the request-response pairs
      grep "\[PROCESSED" "server_${server_type}.log" | 
        sed -E 's/.*Request: ([^ |]+) \| Response: ([^ ]+).*/\1 \2/' > "req_resp_${server_type}.txt"
      
      # Check for correct word reversals
      echo "Checking word reversals..."
      local INCORRECT=0
      
      # Process each pair to check for correct reversals without using associative arrays
      while read -r word reversed; do
        if [[ -n "$word" && -n "$reversed" ]]; then
          expected=""
          
          case "$word" in
            "apple")
              expected="elppa"
              ;;
            "banana")
              expected="ananab"
              ;;
            "cherry")
              expected="yrrehc"
              ;;
            "dragon")
              expected="nogard"
              ;;
            "elephant")
              expected="tnahpele"
              ;;
            *)
              expected=""
              ;;
          esac
          
          if [[ -n "$expected" && "$expected" != "$reversed" ]]; then
            echo "  ⚠️  Race condition found: word '$word' was reversed to '$reversed' instead of '$expected'"
            INCORRECT=$((INCORRECT + 1))
          fi
        fi
      done < "req_resp_${server_type}.txt"
      
      if [ "$INCORRECT" -gt 0 ]; then
        echo "⚠️  Race conditions detected! $INCORRECT requests had incorrect reversals."
      else
        echo "✅ No race conditions detected. All words were correctly reversed."
      fi
      
      # Check for concurrent handling (which would indicate race conditions in the race server)
      if [ "$server_type" == "race" ]; then
        # Create a timeline of operations
        grep -E "\[(RECEIVED|PROCESSED|RESPONDED)\]" "server_${server_type}.log" | sort -k1,2 > "timeline_${server_type}.txt"
        
        # Check if there are overlapping operations
        echo "Checking for overlapping operations..."
        
        local prev_state=""
        local prev_word=""
        local prev_time=""
        local overlaps=0
        
        while read -r line; do
          # Fixed AWK expressions to properly escape the brackets in field separator
          local curr_time=$(echo "$line" | awk -F'\\[|\\]' '{print $2}')
          local curr_state=$(echo "$line" | awk -F'\\[|\\]' '{print $6}')
          local curr_word=$(echo "$line" | awk -F'Request: ' '{print $2}' | awk -F' \\|' '{print $1}')
          
          if [[ "$prev_state" == "RECEIVED" && "$curr_state" == "RECEIVED" && "$curr_time" == "$prev_time" ]]; then
            echo "  ⚠️  Potential race condition: Concurrent processing of '$prev_word' and '$curr_word'"
            overlaps=$((overlaps + 1))
          fi
          
          prev_state="$curr_state"
          prev_word="$curr_word"
          prev_time="$curr_time"
        done < "timeline_${server_type}.txt"
        
        if [ "$overlaps" -gt 0 ]; then
          echo "⚠️  Race condition evidence: Found $overlaps instances of concurrent processing."
        fi
      fi
    else
      echo "‼️  No processed requests found in log. This indicates a serious issue with the server."
    fi
    
    # Print sample log entries for verification
    echo "Sample log entries (last 10):"
    tail -n 10 "server_${server_type}.log" || echo "No log entries found."
  else
    echo "ERROR: Log file server_${server_type}.log not found!"
  fi
  
  echo "====================================="
  echo ""
  
  # Clean up temporary files
  rm -f "req_resp_${server_type}.txt" "timeline_${server_type}.txt" 2>/dev/null
  
  # Wait before testing the next server
  sleep 2
}

# Main program
echo "Starting race condition testing..."

# Ensure run_clients is compiled
echo "Compiling run_clients..."
gcc -o run_clients run_clients.c

# Test race condition server on port 8080
test_server "race"

# Test fixed server on port 8081
test_server "fixed"

echo "Testing complete!"
echo "====================================="
echo "Summary:"
echo "1. RACE Server: Uses a shared buffer without synchronization + 100ms delay"
echo "2. FIXED Server: Uses a semaphore to protect critical section + same 100ms delay"
echo "Check clients.log, server_race.log, and server_fixed.log for detailed logs."
echo "====================================="

# Clean up
cleanup
