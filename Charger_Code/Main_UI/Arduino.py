# Arduino.py

import threading
import time
from collections import deque
import tkinter as tk


def send_scan_command(arduino_socks, arduino_socket_q, root, callback_show_bike_options, callback_back_to_scan_screen):
    """Function to send the 'SCAN' command and process the bike options."""
    try:
        if arduino_socks:
            # Send the SCAN command
            arduino_socks.send(b"SCAN\n")
            threading.Thread(target=scan_for_bikes, args=(arduino_socket_q, root, callback_show_bike_options, callback_back_to_scan_screen)).start()
        else:
            print("Error: Arduino socket connection not available.")
    except Exception as e:
        print(f"Error: {e}")


# Function to handle the scanning process
def scan_for_bikes(arduino_socket_q, root, callback_show_bike_options, callback_back_to_scan_screen):
    """Function to process the bike scanning process and display bike options."""
    try:
        start_time = time.time()

        while True:
            if time.time() - start_time > 5:
                callback_back_to_scan_screen("Scanning failed")
                break

            if arduino_socket_q:
                line = arduino_socket_q.popleft().strip()
                print("ARscript: line: " + line)

                if "Scanning for devices..." in line:
                    continue

                elif "Bikes are available to connect:" in line:
                    time.sleep(1)
                    if arduino_socket_q:
                        line = arduino_socket_q.popleft().strip()
                        print("line: " + line)
                        num_bikes = int(line.split()[0])
                        print("EV_Script: Number of bikes: ", num_bikes)
                        bike_details = []

                        for _ in range(num_bikes):
                            if arduino_socket_q:
                                bike_name = arduino_socket_q.popleft().strip()
                                bike_details.append(bike_name)

                    # Once bike details are ready, show them
                    callback_show_bike_options(bike_details)
                    break
    except Exception as e:
        callback_back_to_scan_screen(f"Error: {e}")
        print(e)


def display_bike_options(bike_details, root, callback_select_bike, callback_back_to_scan_screen):
    """Function to display available bike options and handle bike selection or going back."""
    for widget in root.winfo_children():
        widget.destroy()

    # Show bike options as buttons
    for idx, bike in enumerate(bike_details):
        btn = tk.Button(root, text=f"{bike}", width=20, command=lambda b=bike: callback_select_bike(b))
        btn.pack(pady=10)

    # Add a Back button to go back to the scan screen
    back_btn = tk.Button(root, text="Back", command=callback_back_to_scan_screen)
    back_btn.pack(pady=20)


def handle_bike_selection(bike, root, callback_start_charging):
    """Handle when a bike is selected."""
    # Proceed with the charging process
    callback_start_charging(bike)
