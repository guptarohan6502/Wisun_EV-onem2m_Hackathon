# send_to_BR.py

import socket
import serial
import time
import select

def sendBR(BR_ser_port,port=6002 ):
    print("BRscript: Starting BR communication script...")
    time.sleep(1)

    try:
        # Create a serial connection - For communicating to Wi-SUN Node
        ser = serial.Serial(BR_ser_port, 9600, timeout=1)  # Timeout added for better responsiveness

        # Socket connection to communicate with the UI script.
        sock_port = port  # Reserve a port for your service.
        socks = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        socks.bind((socket.gethostname(), sock_port))  # Bind to the port
        socks.listen(5)
        print(f"BRscript: Listening for incoming connections on port {sock_port}...")

        while True:
            # Establish connection with client.
            clientsocket, cli_address = socks.accept()
            print("BRscript: Got a connection from %s" % str(cli_address))

            if ser.is_open:
                print("BRscript: Socket connection and Serial port are open.")

                inputs = [ser, clientsocket]

                while True:
                    try:
                        readable, _, exceptional = select.select(inputs, [], inputs, 1)  # Timeout for select

                        for source in readable:
                            if source is ser:
                                data = ser.readline().decode().strip()
                                if data:
                                    print(f"BRscript: Received from serial: {data}")
                                    clientsocket.send(data.encode())

                            elif source is clientsocket:
                                data = clientsocket.recv(1024).decode().strip()
                                if data:
                                    print(f"BRscript: Received from client: {data}")
                                    ser.write((data + '\n').encode())

                        for source in exceptional:
                            print("BRscript: An exceptional condition occurred.")
                            inputs.remove(source)
                            source.close()
                            break

                    except Exception as e:
                        print(f"BRscript: Exception: {e}")
                        clientsocket.close()
                        break

            else:
                print("BRscript: Failed to open serial port.")
                break

    except serial.SerialException as e:
        print(f"BRscript: SerialException: {e}")
    except socket.error as e:
        print(f"BRscript: Socket error: {e}")
    finally:
        if 'ser' in locals() and ser.is_open:
            ser.close()
        print("BRscript: Serial port closed.")
        if 'clientsocket' in locals():
            clientsocket.close()
        print("BRscript: Client socket closed.")
