import RPi.GPIO as GPIO
import time

# Define pins for rows and columns of the keypad
L1 = 5
L2 = 6
L3 = 13
L4 = 19

C1 = 12
C2 = 16
C3 = 20
C4 = 21

GPIO.setwarnings(False)
GPIO.setmode(GPIO.BCM)

# Setup the rows as outputs
GPIO.setup(L1, GPIO.OUT)
GPIO.setup(L2, GPIO.OUT)
GPIO.setup(L3, GPIO.OUT)
GPIO.setup(L4, GPIO.OUT)

# Setup the columns as inputs with pull-down resistors
GPIO.setup(C1, GPIO.IN, pull_up_down=GPIO.PUD_DOWN)
GPIO.setup(C2, GPIO.IN, pull_up_down=GPIO.PUD_DOWN)
GPIO.setup(C3, GPIO.IN, pull_up_down=GPIO.PUD_DOWN)
GPIO.setup(C4, GPIO.IN, pull_up_down=GPIO.PUD_DOWN)

key_pressed = False  # To avoid multiple detections for a single key press

class Keypad:
    def __init__(self, on_submit):
        self.on_submit = on_submit  # Callback for key press

    def readLine(self, line, characters):
        """Check each row for a keypress and call the callback if detected."""
        global key_pressed
        GPIO.output(line, GPIO.HIGH)
    
        if GPIO.input(C1) == 1 and not key_pressed:
            key_pressed = True
            self.handle_keypress(characters[0])
        elif GPIO.input(C2) == 1 and not key_pressed:
            key_pressed = True
            self.handle_keypress(characters[1])
        elif GPIO.input(C3) == 1 and not key_pressed:
            key_pressed = True
            self.handle_keypress(characters[2])
        elif GPIO.input(C4) == 1 and not key_pressed:
            key_pressed = True
            self.handle_keypress(characters[3])
    
        GPIO.output(line, GPIO.LOW)

    def handle_keypress(self, char):
        """Handle the keypress and invoke the callback if '#' is pressed."""
        
        self.on_submit(char)  # Trigger the connection when '#' is pressed
        # Reset key_pressed to allow for further input
        time.sleep(0.3)
        global key_pressed
        key_pressed = False

    def check_keypad(self):
        """Continuously check the keypad for input."""
        while True:
            self.readLine(L1, ["1", "2", "3", "A"])
            self.readLine(L2, ["4", "5", "6", "B"])
            self.readLine(L3, ["7", "8", "9", "C"])
            self.readLine(L4, ["*", "0", "#", "D"])

            # Small delay to avoid bouncing
            time.sleep(0.1)

    def cleanup(self):
        """Clean up GPIO settings."""
        GPIO.cleanup()

# Example usage
if __name__ == "__main__":
    def on_submit(char):
        print(f"Key {char} pressed! Starting connection...")

    keypad = Keypad(on_submit)
    try:
        keypad.check_keypad()
    except KeyboardInterrupt:
        keypad.cleanup()
