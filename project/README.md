# Multi-User Communicating Shells

This project implements a terminal-like application with multiple shell instances that can execute commands and communicate via shared memory.

## Features
- Execute shell commands (ls, grep, etc.)
- Send messages between shells using `@msg` command
- Shared message buffer with synchronization
- GTK-based GUI interface
- MVC architecture

## Build Instructions

1. Install dependencies:
   ```bash
   sudo apt-get install build-essential libgtk-3-dev
   
   
   
Test:
	Normal bir komut çalıştırma: (ls -l)  +


	Mesaj gönderme: (@msg Merhaba dünya)  +


	Redirection testi:  ls > output.txt  +

	Pipe testi:  (ps aux | grep bash)  +




