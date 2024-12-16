#!/bin/bash

DEVICE="/dev/queue"
if [ ! -e "$DEVICE" ]; then
    echo "Error: Device $DEVICE doesn't exists."
    exit 1
fi

echo "Starting testing of $DEVICE..."

# Последовательное тестирование
for i in $(seq 1 50); do
    # Чтение результата из устройства
    input=$((i*3))
    echo "enqueue $input" | tee "$DEVICE" > /dev/null
    if [ $? -ne 0 ]; then
        echo "Unable to write: $input"
        exit 2
    fi
done

for i in $(seq 1 50); do
    input=$((i*3))
    # Чтение результата из устройства
    result=$(cat "$DEVICE")
    if [ $? -ne 0 ]; then
        echo "Unable to read"
        exit 3
    fi

    if [ "$result" -ne "$input" ]; then
        echo "Error during sequential read. Expected = $input, actual: $result"
        exit 4
    fi

    # Удаляем элемент из очереди
    echo "dequeue" | tee "$DEVICE" > /dev/null
    if [ $? -ne 0 ]; then
        echo "Unable to write: $input"
        exit 2
    fi
done

echo "Sequential test completed"

# Параллельное тестирование
for i in $(seq 1 50); do
    input=$((i*3))
    (echo "enqueue $input" | tee "$DEVICE" > /dev/null) &
done
wait

for i in $(seq 1 50); do
    input=$((i*3))
    # Чтение результата из устройства
    result=$(cat "$DEVICE")
    if [ $? -ne 0 ]; then
        echo "Unable to read"
        exit 3
    fi

    # Удаляем элемент из очереди
    echo "dequeue" | tee "$DEVICE" > /dev/null
    if [ $? -ne 0 ]; then
        echo "Unable to write: $input"
        exit 2
    fi
done

echo "Parallel test completed"

echo "Testing completed successfully!"
exit 0