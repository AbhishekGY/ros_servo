# Servo Control System

A servo motor control system built on top of the ros-like-system framework. This project provides a Linux kernel module for servo control and a ROS-like node for publishing servo states.

## Prerequisites

- Linux system with kernel headers installed
- Python 3.7+
- `tty0tty` kernel module (for virtual serial ports)
- [ros-like-system](https://github.com/AbhishekGY/ros_like_system.git) installed in your Python environment

## Installation

### 1. Set up tty0tty (Virtual Serial Port Driver)
```bash
# Clone tty0tty repository
git clone https://github.com/freemed/tty0tty.git
cd tty0tty/module

# Build the module
make

# Load the module
sudo cp tty0tty.ko /lib/modules/$(uname -r)/kernel/drivers/misc/
sudo depmod
sudo modprobe tty0tty

# Set permissions
sudo chmod 666 /dev/tnt*
```

### 2. Clone this repository:
```bash
git clone https://github.com/AbhishekGY/ros_servo.git
cd ros_servo
```

### 3. Setup Python virtual environment and install requirements:
```bash
python -m venv venv
source venv/bin/activate
pip install -r requirements.txt
```

### 4. Build the kernel module:
```bash
cd module
make
cd ..
```

### 5. Run the setup script:
```bash
sudo ./setup.sh
```

## Components

### Kernel Module (servo_driver.c)
- Custom line discipline for serial communication
- Character device driver for servo control
- Non-blocking I/O for real-time updates

### Servo Publisher (servo_publisher.py)
- Interfaces with the kernel module
- Publishes servo positions to the ROS network
- Real-time state updates

### ROS-like System
- Provides the core publish/subscribe infrastructure
- Handles network communication between nodes
- Includes robot visualization capabilities
- Installation handled via requirements.txt
- Visit [ros-like-system repository](https://github.com/yourusername/ros-like-system) for more details


## Usage

1. Start the system (this will load the kernel module and start all necessary processes):
```bash
sudo ./setup.sh
```

2. To run in simulation mode without loading the kernel module:
```bash
./setup.sh --no-driver
```

3. To visualize the servo movement, use the visualization tools provided by ros-like-system.

## File Structure
```
.
├── module/
│   ├── Makefile
│   └── servo_driver.c
├── servo_publisher.py
├── setup.sh
├── requirements.txt
└── README.md
```

## Contributing

1. Fork the repository
2. Create your feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add some amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## License

This project is licensed under the GPL License - see the LICENSE file for details.
