import tkinter as tk
import socket
import threading
from collections import deque
from keypad import Keypad
from wisun_set_script import setup_wisun
import Arduino  # Import the Arduino.py functions
import Charger_script
import time

# Create a class to store the result of threads
class ThreadWithReturnValue(threading.Thread):
    def __init__(self, group=None, target=None, name=None, args=(), kwargs={}, verbose=None):
        threading.Thread.__init__(self, group, target, name, args, kwargs)
        self._return = None

    def run(self):
        if self._target is not None:
            self._return = self._target(*self._args, **self._kwargs)

    def join(self, timeout=None):
        threading.Thread.join(self, timeout)
        return self._return


class MainApp:
    def __init__(self, root, wisun_port, arduino_port):
        self.root = root
        self.root.geometry("600x400")
        self.root.title("EV Charger Wi-SUN Connection")

        self.wisun_port = wisun_port
        self.arduino_port = arduino_port

        # Set up the Wi-SUN socket
        self.cli_socks = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.cli_socks.connect((socket.gethostname(), self.wisun_port))  # Bind to the port
        print("EVscript: Connected to Wi-SUN server.")
        self.wisun_socket_q = deque()
        read_wisun_thread = threading.Thread(target=self.read_socket)
        read_wisun_thread.start()

        # Emergency Vehicle
        self.Emergency_vehicle_discovered = False

        # Set up the Arduino socket
        self.arduino_socks = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.arduino_socks.connect((socket.gethostname(), self.arduino_port))  # Bind to the port
        self.arduino_socket_q = deque()
        read_arduino_thread = threading.Thread(target=self.read_Ard_serial)
        read_arduino_thread.start()

        # Create an initial frame
        self.show_initial_frame("Press '#' on the keypad to Connect")
        self.current_frame = "wisun_connect"
        self.accept_keypad_input = True

        # Start keypad input monitoring
        self.keypad = Keypad(self.process_keypad_input)
        self.start_keypad_listener()

        # Define a bike
        self.bike = None

    def read_socket(self):
        """Function to read from sockets."""
        while True:
            msg = self.cli_socks.recv(1024)
            if msg:
                self.wisun_socket_q.append(msg.decode().strip())

    def Emergency_status(self):
        time.sleep(10)
        self.Emergency_vehicle_discovered = False
        return 1

    def read_Ard_serial(self):
        while True:
            msg = self.arduino_socks.recv(1024).decode().strip()

            # Condition 1: Check if it is an emergency message
            if "Emergency:" in msg:
                if not self.Emergency_vehicle_discovered:
                    print("EVscript: Emergency vehicle discovered")
                    self.cli_socks.send(
                        ("wisun socket_write 4 \"" + str((str(msg) + str(" Near Node 1")) + "\"\n")).encode())
                    self.Emergency_vehicle_status_thread = threading.Thread(target=self.Emergency_status)
                    self.Emergency_vehicle_discovered = True
                    self.Emergency_vehicle_status_thread.start()

            # Condition 2: Check if it is an EV_Bike message
            elif "EV_Bike:" in msg:
                clean_msg = msg.replace("EV_Bike: ", "")
                self.arduino_socket_q.append(clean_msg)
                print("UIscript: Arduino q appended msg: " + clean_msg)

            # Condition 3: If anything else, just ignore it
            else:
                pass

    def start_keypad_listener(self):
        """Run the keypad check loop in a separate thread."""
        keypad_thread = threading.Thread(target=self.keypad.check_keypad)
        keypad_thread.daemon = True  # Ensure it stops when the program exits
        keypad_thread.start()

    def process_keypad_input(self, char):
        """Handle keypad input."""
        print(f"UIscript: keypad is processing {char} input")
        if self.accept_keypad_input:
            if self.current_frame == "input_frame":
                current_text = self.entry.get()
                if char == '#':
                    print(f"Submitted Input: {current_text}")
                    self.entry.delete(0, tk.END)
                    self.but_startcharge(int(current_text), self.bike)
                elif char == 'C':
                    self.entry.delete(0, tk.END)
                else:
                    self.entry.insert(tk.END, char)
            elif self.current_frame == "get_id_frame":
                current_text = self.entry.get()
                if char == '#':
                    print(f"ID Entered: {current_text}")
                    self.bike = None  # Set the bike to the entered ID
                    self.entry.delete(0, tk.END)
                    self.show_input_frame()  # Proceed to the amount input frame
                elif char == 'C':
                    self.entry.delete(0, tk.END)
                else:
                    self.entry.insert(tk.END, char)
            elif self.current_frame == "scanning_frame":
                if char == '1':
                    self.accept_keypad_input = False
                    self.send_scan_command()
                elif char == '2':
                    self.accept_keypad_input = False
                    self.show_get_id_frame()
            else:
                if char == '#':
                    print("UIscript: # is pressed")
                    self.accept_keypad_input = False
                    if self.current_frame == "scanning_frame":
                        self.send_scan_command()
                    elif self.current_frame == "ble_bikes":
                        self.send_scan_command()
                    elif self.current_frame == "wisun_connect":
                        self.connect_to_wisun()
                    else:
                        self.send_scan_command()
                elif char == 'C':
                    self.accept_keypad_input = False
                    print("UIscript: C is pressed")
                    if self.current_frame == "ble_bikes":
                        self.show_scanning_frame()
                    elif self.current_frame == "loading_screen":
                        self.connect_to_wisun()
                    elif self.current_frame == "charging_frame":
                        self.arduino_socks.send(b"DISCONNECT\n")
                        self.show_scanning_frame()
                    else:
                        self.show_scanning_frame()
                elif char in ["1", "2", "3"]:  # Select a bike if options are displayed
                    if self.current_frame == "ble_bikes":
                        self.accept_keypad_input = False
                        print(f"UIscript: char {char} is pressed")
                        bike_index = int(char) - 1
                        if bike_index < len(self.bike_details):
                            self.handle_bike_selection(self.bike_details[bike_index])

    def update_status(self, message):
        """Update the status label."""
        self.status_label.config(text=message)
        self.root.update()

    def connect_to_wisun(self):
        """Trigger the Wi-SUN connection setup."""
        self.show_loading_frame("Connecting to Wi-SUN...")
        wisun_thread = threading.Thread(target=self.setup_wisun_and_update)
        wisun_thread.start()

    def show_initial_frame(self, msg):
        """Show the initial frame with a status label."""
        self.clear_frame()
        self.initial_frame = tk.Frame(self.root)
        self.initial_frame.pack(expand=True)
        self.current_frame = "wisun_connect"
        self.accept_keypad_input = True
        self.wisun_connect_button = tk.Button(self.initial_frame, text="Connect to Wisun..(#)", font=("Helvetica", 16))
        self.wisun_connect_button.pack(pady=20)
        self.status_label = tk.Label(self.initial_frame, text=msg, font=("Helvetica", 10))
        self.status_label.pack(pady=20)

    def show_loading_frame(self, message):
        """Show a loading frame."""
        self.clear_frame()
        self.loading_frame = tk.Frame(self.root)
        self.loading_frame.pack(expand=True)
        self.loading_label = tk.Label(self.loading_frame, text=message + " (#)", font=("Helvetica", 16))
        self.accept_keypad_input = True
        self.current_frame = "loading_screen"
        self.loading_label.pack(pady=100)

    def show_scanning_frame(self, msg=""):
        """Show the frame to scan for bikes."""
        self.clear_frame()
        self.scan_frame = tk.Frame(self.root)
        self.scan_frame.pack(expand=True)
        self.scan_button = tk.Button(self.scan_frame, text="Start Scanning (1)", font=("Helvetica", 16))
        self.scan_button.pack(pady=20)
        self.enter_id_button = tk.Button(self.scan_frame, text="Enter ID manually (2)", font=("Helvetica", 16))
        self.enter_id_button.pack(pady=20)
        self.status_label = tk.Label(self.scan_frame, text="Press '#' to scan for bikes.\n" + msg, font=("Helvetica", 10))
        self.accept_keypad_input = True
        self.current_frame = "scanning_frame"
        self.status_label.pack(pady=20)

    def show_get_id_frame(self):
        """Show a frame where the user can enter a bike ID manually."""
        self.clear_frame()

        self.get_id_frame = tk.Frame(self.root)
        self.get_id_frame.pack(expand=True)

        # Create an entry widget to display the entered ID
        self.entry = tk.Entry(self.get_id_frame, width=20, font=("Helvetica", 16))
        self.entry.pack(pady=20)

        # Create a label for instructions
        label = tk.Label(self.get_id_frame, text="Enter bike ID via keypad", font=("Helvetica", 16))
        label.pack(pady=10)

        buttons = [
            ["1", "2", "3", "A"],
            ["4", "5", "6", "B"],
            ["7", "8", "9", "C"],
            ["*", "0", "#", "D"]
        ]

        for row in buttons:
            button_row = tk.Frame(self.get_id_frame)
            button_row.pack()
            for char in row:
                btn = tk.Button(button_row, text=char, font=("Helvetica", 16))
                btn.pack(side=tk.LEFT)

        self.current_frame = "get_id_frame"
        self.accept_keypad_input = True

    def send_scan_command(self):
        """Send the SCAN command to the Arduino."""
        Arduino.send_scan_command(self.arduino_socks, self.arduino_socket_q, self.root, self.display_bike_options,
                                  self.show_scanning_frame)

    def display_bike_options(self, bike_details):
        """Display the bike options."""
        self.bike_details = bike_details  # Save bike details for further reference
        self.accept_keypad_input = True
        self.current_frame = "ble_bikes"
        Arduino.display_bike_options(self.bike_details, self.root, self.handle_bike_selection, self.show_scanning_frame)

    def handle_bike_selection(self, bike):
        """Handle the selected bike and start charging."""
        self.show_loading_frame(f"Starting charging for {bike}...")
        self.bike = bike
        self.show_input_frame()

    def show_input_frame(self):
        """We will take amount input in this frame"""
        self.clear_frame()

        self.input_amount_frame = tk.Frame(self.root)
        self.input_amount_frame.pack(expand=True)

        # Create an entry widget to display input
        self.entry = tk.Entry(self.input_amount_frame, width=20, font=("Helvetica", 16))
        self.entry.pack(pady=20)

        # Create a label for instructions
        label = tk.Label(self.input_amount_frame, text="Enter amount via keypad", font=("Helvetica", 16))
        label.pack(pady=10)

        buttons = [
            ["1", "2", "3", "A"],
            ["4", "5", "6", "B"],
            ["7", "8", "9", "C"],
            ["*", "0", "#", "D"]
        ]

        for row in buttons:
            button_row = tk.Frame(self.input_amount_frame)
            button_row.pack()
            for char in row:
                btn = tk.Button(button_row, text=char, font=("Helvetica", 16))
                btn.pack(side=tk.LEFT)

        self.current_frame = "input_frame"
        self.accept_keypad_input = True

    def check_rfid_valid(self, idtag_str):
        print(f"EVscript: Input Validation started")
        print(f"EVscript: data being sent")

        read_string = ""
        self.cli_socks.send(("wisun socket_write 4 \"" + idtag_str + "\"\n").encode())
        print("EVscript: wisun socket_write 4 \"" + idtag_str + "\"\n")

        if self.wisun_socket_q:
            read_string = self.wisun_socket_q.popleft().strip()
            print(f"read_string:{read_string}")

        while "valid" not in str(read_string):
            if self.wisun_socket_q:
                read_string = self.wisun_socket_q.popleft().strip()
                print(f"read_string:{read_string}")
            else:
                continue
            print("\r", f"Charger: waiting For ID validation", end='\r')
        print(f"Charger: Validation done")

        if "valid_yes" in str(read_string):
            return True
        elif "valid_not" in str(read_string):
            return False
        elif "valid_insuff" in str(read_string):
            return "Low balance"
        elif "valid_error" in str(read_string):
            return "OneM2M Not responding at the moment"

    def charger_func(self, amount):
        """Simulates the charging process."""
        print(f"Amount: {amount}")
        idtag_str = f"{{'Amount': {amount}, 'VehicleidTag': '12345678', 'Time': {time.time()}, 'Chargerid': 'EV-L001-03'}}"
        try:
            RFID_thread = ThreadWithReturnValue(target=self.check_rfid_valid, args=(idtag_str,))
            RFID_thread.start()
            Rfid_valid = RFID_thread.join(timeout=60)  # 1 minute to verify RFID
            print(f"RFID Validity: {Rfid_valid}")
            time.sleep(2)  # Simulate charging time
            charge_status = Charger_script.Charger(Rfid_valid, amount)
            print(f"UIscript: Charging encountered status {charge_status}")
            return charge_status
        except Exception as e:
            print(f"Error in charging: {e}")
            return 5  # Error code

    def but_startcharge(self, amount, bike):
        """Function to start charging."""
        print(f"Charging started for {bike}!")

        if amount is None:
            self.show_loading_frame("Error: Invalid amount. Please try again.")
        else:
            # Update the UI to show charging screen
            self.clear_frame()
            self.current_frame = "charging_frame"
            self.accept_keypad_input = True

            self.charging_frame = tk.Frame(self.root)
            self.charging_frame.pack(expand=True)
            self.charging_label = tk.Label(self.charging_frame, text="Charging in Progress....", font=("Helvetica", 10))
            self.charging_label.pack(pady=20)
            self.scan_button = tk.Button(self.charging_frame, text="Cancel (C)", font=("Helvetica", 16))
            self.scan_button.pack(pady=20)

            # Perform the background steps
            try:
                if bike is None:
                    charging_status = self.charger_func(amount)
                    # Handle the post-charging process
                    if charging_status == 1:
                        self.arduino_socks.send(b"DISCONNECT\n")
                        self.show_charging_completed()
                    else:
                        self.arduino_socks.send(b"DISCONNECT\n")
                        self.show_loading_frame("Error: Charging Failed")

                else:
                    # Find the index of the bike in the bike details list (1-based indexing)
                    index = self.bike_details.index(bike) + 1 if bike and self.bike_details else 0

                    # Write the Arduino indexing to the Arduino socket
                    self.arduino_socks.send(str(index).encode())
                    self.bike = None
                    time.sleep(2)
                    # Read two lines from the Arduino serial queue (peripheral name and address)
                    if len(self.arduino_socket_q) >= 2:
                        peripheral_name = self.arduino_socket_q.popleft().strip()
                        peripheral_address = self.arduino_socket_q.popleft().strip()

                        # Print peripheral details for reference
                        print(f"EV_script: Peripheral Name: {peripheral_name}")
                        print(f"EV_script: Peripheral Address: {peripheral_address}")

                        # Call the charging function
                        charging_status = self.charger_func(amount)

                        # Handle the post-charging process
                        if charging_status == 1:
                            self.arduino_socks.send(b"DISCONNECT\n")
                            self.show_charging_completed()
                        else:
                            self.arduino_socks.send(b"DISCONNECT\n")
                            self.show_loading_frame("Error: Charging Failed")
                    else:
                        self.arduino_socks.send(b"DISCONNECT\n")
                        print("EV_script: Error: Not enough data in the queue to read peripheral details.")
                        self.show_loading_frame("Error: Could not retrieve peripheral details.")

            except Exception as e:
                self.arduino_socks.send(b"DISCONNECT\n")
                print(f"EV_script: Error in but_startcharge: {e}")
                self.show_loading_frame(f"Error: {e}")

    def show_charging_completed(self):
        """Display 'Charging Completed' for 2 seconds, then go back to the scanning frame."""
        self.clear_frame()

        # Show 'Charging Completed' message
        self.charging_completed_frame = tk.Frame(self.root)
        self.charging_completed_frame.pack(expand=True)
        self.charging_completed_label = tk.Label(self.charging_completed_frame, text="Charging Completed", font=("Helvetica", 16))
        self.charging_completed_label.pack(pady=20)

        # Delay for 2 seconds before returning to the scanning frame
        self.root.after(2000, self.show_scanning_frame)

    def setup_wisun_and_update(self):
        """Setup Wi-SUN and update the UI based on success or failure."""
        set_wisun_thread = ThreadWithReturnValue(target=setup_wisun, args=(self.cli_socks, self.wisun_socket_q))
        set_wisun_thread.start()
        connected = set_wisun_thread.join(timeout=600)  # Timeout in case Wi-SUN setup takes too long

        if connected:
            self.show_scanning_frame()  # If Wi-SUN setup is successful, move to scanning
        else:
            self.show_initial_frame("Failed to connect to Wi-SUN.\nPress '#' on the keypad to try again.")

    def clear_frame(self):
        """Clear all widgets from the main window."""
        for widget in self.root.winfo_children():
            widget.destroy()


