#!/bin/bash

# IRC Server DCC File Transfer Test Script
# Usage: ./test_dcc.sh

PORT=6667
PASSWORD="test123"
SERVER="localhost"

echo "=== IRC Server DCC File Transfer Test ==="
echo "Port: $PORT"
echo "Password: $PASSWORD"
echo ""

# Create test files
echo "Creating test files..."
mkdir -p dcc_transfers/received

# Create a small test file
echo "This is a small test file for DCC transfer testing." > dcc_transfers/small_test.txt
echo "It contains some sample data." >> dcc_transfers/small_test.txt
echo "Testing DCC SEND functionality." >> dcc_transfers/small_test.txt

# Create a medium test file (1KB)
dd if=/dev/urandom of=dcc_transfers/medium_test.bin bs=1024 count=1 2>/dev/null
echo "Created medium_test.bin (1KB)"

# Create a larger test file (10KB) 
dd if=/dev/urandom of=dcc_transfers/large_test.bin bs=1024 count=10 2>/dev/null
echo "Created large_test.bin (10KB)"

echo ""
echo "Test files created in ./dcc_transfers/"
echo ""

# Test client 1 (Sender)
cat << 'EOF' > /tmp/irc_client1.txt
PASS test123
NICK Alice
USER alice 0 * :Alice User
PRIVMSG Bob :Hello Bob! Let me send you a file.
DCC SEND Bob ./dcc_transfers/small_test.txt
DCC LIST
DCC STATUS
QUIT :Test complete
EOF

# Test client 2 (Receiver)
cat << 'EOF' > /tmp/irc_client2.txt
PASS test123
NICK Bob
USER bob 0 * :Bob User
PRIVMSG Alice :Hi Alice! Ready to receive.
DCC LIST
DCC STATUS
QUIT :Test complete
EOF

echo "=== Test Commands ==="
echo ""
echo "1. Start the IRC server:"
echo "   ./ircserv $PORT $PASSWORD"
echo ""
echo "2. In terminal 1 (Alice - Sender):"
echo "   nc $SERVER $PORT < /tmp/irc_client1.txt"
echo ""
echo "3. In terminal 2 (Bob - Receiver):"
echo "   nc $SERVER $PORT"
echo "   Then type:"
echo "   PASS $PASSWORD"
echo "   NICK Bob"
echo "   USER bob 0 * :Bob User"
echo "   (Wait for DCC notification)"
echo "   DCC LIST"
echo "   DCC ACCEPT <transferId>"
echo "   DCC STATUS"
echo ""
echo "4. Monitor transfer progress with:"
echo "   DCC STATUS"
echo ""
echo "=== Manual Testing ==="
echo ""
echo "You can also test manually with nc:"
echo "nc $SERVER $PORT"
echo ""
echo "Then use these commands:"
echo "- PASS $PASSWORD"
echo "- NICK YourNick"
echo "- USER youruser 0 * :Your Name"
echo "- DCC SEND <target_nick> <filepath>"
echo "- DCC LIST"
echo "- DCC ACCEPT <transferId>"
echo "- DCC REJECT <transferId>"
echo "- DCC CANCEL <transferId>"
echo "- DCC STATUS"
echo ""
echo "=== File Locations ==="
echo "- Send files from: ./dcc_transfers/"
echo "- Received files: ./dcc_transfers/received/"
echo ""
