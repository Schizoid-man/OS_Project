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

  echo "====================================="
  echo "Test completed for $server_type server"
  echo "====================================="
  echo ""

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

# Clean up
cleanup
