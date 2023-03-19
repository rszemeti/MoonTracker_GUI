import serial

ser = serial.Serial(
    port='COM9',\
    baudrate=115200,\
    parity=serial.PARITY_NONE,\
    stopbits=serial.STOPBITS_ONE,\
    bytesize=serial.EIGHTBITS,\
    timeout=0)

print("connected to: " + ser.portstr)


#this will store the line
line = []

while  True:
    for c in ser.read():
        line.append(chr(c))
        if c == 10:
            print("Line: " + ''.join(line))
            line = []
            break
        

ser.close()
