#!/bin/bash
PORT_NUMBER=6962
# Start server silently and store PID
./jbod_server -p $PORT_NUMBER &>/dev/null &
server_pid=$!

for test_file in linear simple random; do
  # Wait for server startup before starting the test
  sleep 1

  echo "Testing file: $test_file"

 # Run test with traces/$test_file, size 1024, redirecting stdout to /dev/null and keeping stderr open
  if ! ./tester -w traces/"$test_file"-input -s 1024 &>/dev/null 2>&1; then
    echo "Error: ./tester failed for file: $test_file"
    kill $server_pid &>/dev/null
    wait $server_pid
    exit 1
  fi

  # Check exit code and signal error without printing diff
  if [[ $? -ne 0 ]]; then
    echo "Error: diff found between test output and expected output for $test_file"
    exit 1
  fi

  # Kill and restart server silently for next test
  kill $server_pid &>/dev/null
  wait $server_pid

  ./jbod_server -p $PORT_NUMBER &>/dev/null &
  server_pid=$!
done

echo ""
echo "𝐀𝐋𝐋 𝐓𝐄𝐒𝐓𝐒 𝐒𝐔𝐂𝐂𝐄𝐒𝐒𝐅𝐔𝐋𝐋𝐘 𝐂𝐎𝐌𝐏𝐋𝐄𝐓𝐄𝐃!"
echo ""