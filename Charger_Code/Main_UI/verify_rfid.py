def check_rfid_valid(idtag_str):
    global start_time
    print(f"EVscript: Input give Validation started")
    print(f"EVscript: data being sent")
    global socket_q
    global cli_socks
    
    read_string = ""
    cli_socks.send(("wisun socket_write 4 \"" + idtag_str + "\"\n").encode())
    print("EVscript: wisun socket_write 4 \"" + idtag_str + "\"\n")
    
    if socket_q:
        read_string = socket_q.popleft().strip()
        print(f"read_string:{read_string}")
    
    while "valid" not in str(read_string):
        if socket_q:
            read_string = socket_q.popleft().strip()
            print(f"read_string:{read_string}")
        else:
            continue
        print("\r", f"Charger: waiting For Id validation", end='\r')
    print(f"Charger: Validation done")

    if "valid_yes" in str(read_string):
        return True
    elif "valid_not" in str(read_string):
        return False
    elif "valid_insuff" in str(read_string):
        return "Low balance"
    elif "valid_error" in str(read_string):
        return "Onem2m Not responding at the Moment"
