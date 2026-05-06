#!/bin/bash

# PRECONDITION: Make sure you have Python 3 installed. The code below works for Mac OS only.
# PRECONDITION: Make the program is written onto the board. 
# PRECONDITION: Adjust the port in "py_serial.py" to the port of your board. Instruction in py_serial.py.

# INSTRUCTIONS:
# 0. On terminal, cd to the github directory.

# 1. Run two lines of code below in terminal to setup the ascii_racer package on a Mac: 
# chmod +x setup_mac.sh
# ./setup_mac.sh

# Check if virtualenv is installed, if not install it.
if ! command -v virtualenv &>/dev/null; then
    echo "virtualenv not found, installing..."
    pip3 install virtualenv
fi

# Create a virtual environment.
virtualenv venv

# Activate the virtual environment.
source venv/bin/activate

# Install necessary libraries.
pip3 install setuptools pyserial pynput

# Prompt user to enter the path to the downloaded github directory
echo "Please enter the full path to github directory you've downloaded:"
read ascii_racer_path

# Change directory to ascii_racer
cd "$ascii_racer_path"

# Install the ascii_racer package.
python3 setup.py install

# Display instructions.
echo ""
echo "" 
echo ""
echo "================== ASCII Racer Instructions ====================="
echo "Avoid the beer cans (BUD) as their alcohol percentage are too low."
echo "Collect the gin and vodka and dollar bills instead!"
echo ""
echo "================== ASCII Racer Controls ====================="
echo " - Steer: Tilt board to hard left or right." 
echo " - Cruise: Level board's tilt to enter dead zone."
echo " - Accelerate: Press the right switch."
echo " - Decelerate: Press the left switch." 
echo " - Reset: Press reset switch."
echo " - Quit: Press 'q' on the keyboard. Ctrl+C to quit the background script."
echo ""
echo "================== ASCII Racer Game Start ====================="
echo "ONCE READY, PRESS THE RESET SWITCH. WAIT FOR THE START LIGHT TO TURN GREEN." 
echo "HAVE FUN DRIVING!"

# Run the script to communication with board. 
python3 py_serial.py

# Deactivate the virtual environment.
deactivate
