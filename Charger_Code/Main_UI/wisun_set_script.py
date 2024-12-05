# wisun_set_script.py

import socket
import time

# def setup_wisun(sock,socket_q):
    # """Function to initiate Wi-SUN setup by communicating with send_to_BR."""
    # print("Starting Wi-SUN setup process...")

    # try:
        # # Create a socket connection to communicate with send_to_BR
        # with sock as s:
            # print(f"Wisun_connect_script: {host} and {port}")
            # s.sendall(b"wisun get wisun\n")
            # time.sleep(0.25)
            # s.sendall(b"wisun join_fan11\n")

            # # Wait for confirmation of Wi-SUN setup
            # data = s.recv(1024).decode().strip()
            # if "IPv6 address" in data or "wisun.border_router" in data:
                # print("Wi-SUN setup complete.")
                # return True
            # else:
                # print("Wi-SUN setup failed.")
                # return False

    # except Exception as e:
        # print(f"Error during Wi-SUN setup: {e}")
        # return False

def setup_wisun(cli_socks,socket_q):


    read_string = ""

    try:
        # Send Wi-SUN join commands through the socket
        time.sleep(0.25)
        cli_socks.send(b"wisun get wisun\n")
        time.sleep(0.25)
        cli_socks.send(b"wisun join_fan11\n")
        time.sleep(0.5)
        print("Wisun_set_script: done")
        if socket_q:
            read_string = socket_q.pop()

        while "IPv6 address" not in str(read_string) and "wisun.border_router = fd12:3456" not in str(read_string):
            if socket_q:
                read_string = socket_q.pop()
            else:
                continue
            print("\r", "Wisun_set_script: waiting For wisun to setup", end='\r')

        if "IPv6 address" in str(read_string) or "wisun.border_router" in str(read_string):
            cli_socks.send(b"wisun udp_server 5001\n")
            time.sleep(1)
            cli_socks.send(b"wisun udp_client fd12:3456::1 5005\n")
            time.sleep(1)
            cli_socks.send(b"wisun get wisun\n")
            time.sleep(1)
            cli_socks.send(b"wisun socket_list\n")
            time.sleep(1)
            return True
        else:
            print("I am here")
            return False

    except Exception as e:
        print("Wisun_set_script: Here at exception")
        print(e)
        return False
