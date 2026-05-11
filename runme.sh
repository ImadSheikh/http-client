#!/bin/bash

echo "=============================="
echo " Compiling HTTP Client"
echo "=============================="

gcc -Wall -Wextra -o client client.c

if [ $? -ne 0 ]; then
    echo "Compilation failed"
    exit 1
fi

echo ""
echo "=============================="
echo " Test 1: Simple GET"
echo "=============================="

./client http://httpbin.org/get

echo ""
echo "=============================="
echo " Test 2: With parameters (-r)"
echo "=============================="

./client -r 3 addr=jerusalem tel=02-6655443 age=23 http://httpbin.org/anything

echo ""
echo "=============================="
echo " Test 3: Redirect test"
echo "=============================="

./client http://httpbin.org/redirect/1

echo ""
echo "=============================="
echo " Test 4: Root path"
echo "=============================="

./client http://httpbin.org

echo ""
echo "=============================="
echo " Test 5: Invalid URL (should fail)"
echo "=============================="

./client google.com

echo ""
echo "=============================="
echo " DONE"
echo "=============================="