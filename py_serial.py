import serial  # from pyserial package
from pynput.keyboard import Controller
import subprocess

counter1 = 0
counter2 = 0

def start_game():
    # AppleScript command to open a new full-size Terminal window and run the asciiracer game.
    command = """
    tell application "Terminal"
        activate
        set newWindow to do script "python3 -m asciiracer"
        set number of rows of newWindow to 200
        set number of columns of newWindow to 200
        set custom title of newWindow to "ASCII Racer"
        tell application "System Events" to keystroke "f" using {command down, control down}
    end tell
    """ 
    subprocess.run(["osascript", "-e", command], check=True) # Run the AppleScript command

#PRECONDITION: Open the serial connection to the microcontroller
            # Use "ls /dev/tty.*" to find the correct port
            # Replace '/dev/cu.usbmodemSDA6C611E511' with the correct port

with serial.Serial('/dev/cu.usbmodemSDA6C611E511', 115200) as ser:
    keyboard = Controller()

    while True:
        input = ser.readline() # Read the serial input
        print(input)

        
        if (input == b'asciiracer\n'): # Quit current game and start a new game
            keyboard.press('q')
            keyboard.release('q')
            print("Starting game...")
            start_game()
        elif (input == b'w\n'): # Instruct car to increase speed
            keyboard.press('w')
            keyboard.release('w')
        elif (input == b's\n'): # Instruct car to decrease speed
            keyboard.press('s')
            keyboard.release('s')
        elif (input == b'a\n'): # Instruction to move car left at every eighth input
            counter1 = counter1 + 1
            if counter1 == 8:  
                keyboard.press('a')
                keyboard.release('a')
                counter1 = 0
        elif (input == b'd\n'): # Instruction to move car right at every eighth input
            counter2 = counter2 + 1
            if counter2 == 8:  
                keyboard.press('d')
                keyboard.release('d')
                counter2 = 0

