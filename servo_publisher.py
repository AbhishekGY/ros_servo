import asyncio
import os
import fcntl
from core.ros_core import NetworkAddress, Message, logger
from nodes.node import Node

class ServoPublisher(Node):
    def __init__(self, name: str, address: NetworkAddress, master_address: NetworkAddress):
        super().__init__(name, address, master_address)
        self.running = True
        self.device_path = "/dev/servo"
        self._device_fd = None
        self.loop = None
        self._read_buffer = ""

    async def setup_device(self):
        """Setup the device file for non-blocking I/O"""
        try:
            # Open device file
            self._device_fd = os.open(self.device_path, os.O_RDONLY | os.O_NONBLOCK)
            
            # Get the event loop
            self.loop = asyncio.get_event_loop()
            
            logger.info(f"Successfully opened {self.device_path}")
            return True
        except Exception as e:
            logger.error(f"Failed to setup device: {e}")
            if self._device_fd is not None:
                os.close(self._device_fd)
                self._device_fd = None
            return False

    async def read_servo_angle(self):
        """Read angle from servo device file asynchronously"""
        if self._device_fd is None:
            return None

        try:
            # Read data asynchronously
            data = await self.loop.run_in_executor(
                None, 
                lambda: os.read(self._device_fd, 1024)
            )
            
            if data:
                # Append to buffer and process
                self._read_buffer += data.decode('utf-8')
                
                # Process complete lines
                while '\n' in self._read_buffer:
                    line, self._read_buffer = self._read_buffer.split('\n', 1)
                    try:
                        return float(line.strip())
                    except ValueError:
                        logger.warning(f"Invalid angle value received: {line}")
                        continue
                        
        except BlockingIOError:
            # No data available right now
            return None
        except Exception as e:
            logger.error(f"Error reading servo angle: {e}")
            return None

    async def publish_servo_state(self):
        """Publish servo state"""
        if not await self.setup_device():
            logger.error("Failed to setup device, exiting...")
            return

        try:
            while self.running:
                angle = await self.read_servo_angle()
                if angle is not None:
                    state = {
                        "joint1_angle": angle * (3.14159 / 180.0),  # Convert to radians
                        "joint2_angle": 0.0  # Fixed angle for second joint
                    }
                    await self.publish("arm_state", state)
                    logger.debug(f"Published servo angle: {angle}")
                
                await asyncio.sleep(1/60)  # 60Hz update rate
                
        except Exception as e:
            logger.error(f"Error in publish loop: {e}")
        finally:
            if self._device_fd is not None:
                os.close(self._device_fd)
                self._device_fd = None

    async def start(self):
        """Start the node and publisher"""
        # Start node
        node_task = await super().start()
        
        # Start publishing task
        publish_task = asyncio.create_task(self.publish_servo_state())
        
        # Wait for both tasks
        await asyncio.gather(node_task, publish_task)

async def main():
    node_address = NetworkAddress("localhost", 0)
    master_address = NetworkAddress("localhost", 11511)
    publisher = ServoPublisher("servo_publisher", node_address, master_address)
    
    try:
        await publisher.start()
    except KeyboardInterrupt:
        publisher.running = False
        publisher.stop()
    except Exception as e:
        logger.error(f"Error running publisher: {e}")
        publisher.stop()

if __name__ == "__main__":
    asyncio.run(main())
