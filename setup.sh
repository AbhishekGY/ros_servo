#!/bin/bash

# Function to check if running as root
check_root() {
    if [ "$EUID" -ne 0 ]; then 
        echo "Please run as root"
        exit 1
    fi
}

# Function to setup servo driver
setup_driver() {
    # Remove if already loaded
    rmmod servo_driver 2>/dev/null
    sleep 1

    # Load the module
    insmod module/servo_driver.ko
    echo "Servo driver loaded successfully"

    # Create device node if doesn't exist
    if [ ! -e "/dev/servo" ]; then
        # Get major number from dmesg
        major=$(dmesg | grep -o "servo.*major:[0-9]*" | grep -o "[0-9]*$" | tail -1)
        mknod /dev/servo c $major 0
    fi

    # Set permissions
    chmod 666 /dev/servo
    echo "Device permissions set"

    # Setup ldattach
    pkill ldattach 2>/dev/null
    sleep 1
    ldattach -s 9600 20 /dev/tnt1 > /dev/null 2>&1 &
    sleep 1
    echo "Started ldattach"
}

# Function to start ROS components
start_ros() {
    # Kill any existing processes
    pkill -f "ros-master" >/dev/null 2>&1
    pkill -f "ros-visualizer" >/dev/null 2>&1
    sleep 1

    source venv/bin/activate

    # Start ROS components
    echo "Starting ROS master..."
    gnome-terminal -- ros-master
    sleep 2

    echo "Starting ROS visualizer..."
    gnome-terminal -- ros-visualizer
    sleep 1

    echo "Starting servo publisher..."
    gnome-terminal -- python servo_publisher.py
}

# Main script
if [ "$1" = "--no-driver" ]; then
    echo "Skipping driver setup"
    start_ros
else
    check_root
    setup_driver
    start_ros
fi

echo "Setup complete! All components are running."
