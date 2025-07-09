# OmniXtend C Implementation

<div align="center">
  <img src="images/omnixtend-logo.png" alt="OmniXtend Logo" width="300">
</div>

A C implementation of the OmniXtend protocol, providing a reliable and efficient TileLink over Ethernet (TLOE) communication framework.

## Overview

This project implements the OmniXtend protocol in C, enabling TileLink-based communication over Ethernet networks. It provides both master and slave endpoint implementations with support for multiple transport fabrics including Ethernet and Message Queue (MQ) modes.

## Features

- **TileLink Protocol Support**: Full implementation of TileLink protocol over Ethernet
- **Dual Transport Modes**: 
  - Ethernet mode with raw socket support
  - Message Queue (MQ) mode for inter-process communication
- **Master/Slave Architecture**: Support for both master and slave endpoint roles
- **Flow Control**: Credit-based flow control mechanism
- **Reliable Communication**: Sequence number management and retransmission support
- **Multi-threaded Design**: Asynchronous processing with dedicated threads
- **Memory Operations**: Read/write memory operations with configurable parameters
- **Network Simulator**: Built-in network simulator (`tloe_ns`) for testing and debugging
- **Debug Support**: Comprehensive debugging and statistics collection

## Project Structure

```
├── tloe_endpoint.c/h      # Main endpoint implementation
├── tloe_connection.c/h    # Connection management
├── tloe_frame.c/h         # Frame formatting and parsing
├── tloe_ether.c/h         # Ethernet transport layer
├── tloe_fabric.c/h        # Fabric abstraction layer
├── tloe_transmitter.c/h   # Transmission logic
├── tloe_receiver.c/h      # Reception logic
├── tloe_seq_mgr.c/h       # Sequence number management
├── flowcontrol.c/h        # Flow control implementation
├── retransmission.c/h     # Retransmission logic
├── tilelink_handler.c/h   # TileLink protocol handler
├── tilelink_msg.c/h       # TileLink message structures
├── timeout.c/h            # Timeout management
├── tloe_ns.c              # Network simulator
├── tloe_ns_thread.c/h     # Network simulator thread
├── tloe_mq.c/h            # Message queue implementation
├── util/                  # Utility functions
│   ├── circular_queue.c/h # Circular buffer implementation
│   └── util.c/h          # General utilities
└── Makefile              # Build configuration
```

## Building

### Prerequisites

- GCC compiler
- POSIX threads library
- Linux kernel headers (for Ethernet mode)

### Compilation

```bash
# Basic build
make

# Build with debug information
make DEBUG=1

# Build with memory checking (AddressSanitizer)
make MEMCHECK=1

# Build with specific test configurations
make TEST_NORMAL_FRAME_DROP=1
make TEST_TIMEOUT_DROP=1
```

### Build Targets

- `tloe_endpoint`: Main endpoint executable
- `tloe_ns`: Network simulator executable
- `tags`: Generate ctags for code navigation

## Usage

### TLOE Endpoint

The main endpoint supports two transport modes:

#### Ethernet Mode

```bash
# Master endpoint
./tloe_endpoint -i <interface> -d <destination_mac> -m

# Slave endpoint  
./tloe_endpoint -i <interface> -d <destination_mac> -s
```

#### Message Queue Mode

```bash
# Master endpoint
./tloe_endpoint -p <queue_name> -m

# Slave endpoint
./tloe_endpoint -p <queue_name> -s
```

### Interactive Commands

Once the endpoint is running, you can use these interactive commands:

- `e` - Start Arty demo mode
- `c` - Open connection
- `d` - Close connection  
- `s` - Show status and statistics
- `r <addr> [count]` - Read memory at address (optional count)
- `w <addr> <value> [count]` - Write value to memory address (optional count)
- `t <count>` - Run test sequence (read/write operations)
- `q` - Quit the program

### Network Simulator (tloe_ns)

The network simulator provides testing and debugging capabilities:

```bash
./tloe_ns -p <queue_name>
```

#### Simulator Commands

- `s` - Toggle simulator thread (start/stop)
- `start/run/r` - Start simulator thread
- `stop/halt/h` - Stop simulator thread
- `flush/f` - Flush message queues
- `a <count>` - Drop next N requests from Port A to B
- `b <count>` - Drop next N requests from Port B to A
- `w <count>` - Drop next N requests bi-directionally
- `quit/q` - Quit simulator

## Configuration

### Compile-time Options

- `DEBUG=1`: Enable debug output
- `TEST_NORMAL_FRAME_DROP=1`: Enable normal frame drop testing
- `TEST_TIMEOUT_DROP=1`: Enable timeout drop testing
- `MEMCHECK=1`: Enable AddressSanitizer for memory checking
- `WDE=1`: Enable WDE (Write Data Enable) mode

### Runtime Parameters

- **Connection Timeout**: `CONN_RESEND_TIME` (microseconds)
- **Window Size**: `WINDOW_SIZE` for retransmission buffer
- **Buffer Sizes**: Configurable queue sizes for different message types
- **Channel Credits**: Default credit values for flow control

## Protocol Details

### Frame Structure

TLOE frames include:
- Header with sequence numbers, acknowledgments, and channel information
- Payload data
- Flow control credits

### Channel Types

- Channel 0: Control messages
- Channel A: TileLink requests
- Channel B: TileLink responses  
- Channel C: TileLink data
- Channel D: TileLink acknowledgments
- Channel E: Reserved

### Flow Control

Credit-based flow control prevents buffer overflow:
- Credits are exchanged during connection establishment
- Credits are consumed when sending messages
- Credits are replenished when receiving acknowledgments

## Development

### Code Style

- C99 standard
- Consistent indentation and naming conventions
- Comprehensive error handling
- Thread-safe implementations

### Debugging

Enable debug output with `DEBUG=1`:
```bash
make DEBUG=1
./tloe_endpoint [options]
```

### Testing

Use the network simulator for testing:
```bash
# Terminal 1: Start network simulator
./tloe_ns -p test_queue

# Terminal 2: Start master endpoint
./tloe_endpoint -p test_queue -m

# Terminal 3: Start slave endpoint  
./tloe_endpoint -p test_queue -s
```

## License

This project is licensed under the Apache License, Version 2.0. See the LICENSE file for details.

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests if applicable
5. Submit a pull request

## Acknowledgments

- Based on the OmniXtend protocol specification
- Implements TileLink over Ethernet communication
- Designed for high-performance, reliable network communication

---

<div align="center">
  <img src="images/ETRI_CI_01.png" alt="ETRI Logo" width="200">
  <br><br>
  <strong>This project is developed and maintained by <a href="https://www.etri.re.kr/">ETRI (Electronics and Telecommunications Research Institute)</a></strong>
</div>