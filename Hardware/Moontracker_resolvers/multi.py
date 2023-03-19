import time
import PySimpleGUI as sg
import serial

def serial_thread(portId, window):

    ser = serial.Serial(
        port=portId,\
        baudrate=115200,\
        parity=serial.PARITY_NONE,\
        stopbits=serial.STOPBITS_ONE,\
        bytesize=serial.EIGHTBITS,\
        timeout=0)

    #this will store the line
    line = []

    while  True:
        for c in ser.read():
            line.append(chr(c))
            if c == 10:
                window.write_event_value(('-THREAD-', ''.join(line)), 'Done!')
                line = []
                break

def the_gui():

    sg.theme('Light Brown 3')

    layout = [[sg.Text('Incoming Data')],
              [sg.Output(size=(70, 12))],
              [sg.Text('Port'),
                  sg.Input(default_text='COM9', key='-PORT-', size=(5, 1)),
                  sg.Button('Connect', bind_return_key=True)],
              [sg.Button('Click Me'), sg.Button('Exit')], ]

    window = sg.Window('Moon Tracker', layout)

    # --------------------- EVENT LOOP ---------------------
    while True:
        event, values = window.read()
        if event in (sg.WIN_CLOSED, 'Exit'):
            break
        elif event == 'Connect':
            portId = values['-PORT-']
            print('Thread ALIVE! Connecting to port {}'.format(portId))
            window.start_thread(lambda: serial_thread(portId, window), ('-THREAD-', '-THREAD ENDED-'))
        elif event == 'Click Me':
            print('Your GUI is alive and well')
        elif event[0] == '-THREAD-':
            print('Data: ', event[1])

    # if user exits the window, then close the window and exit the GUI func
    window.close()

if __name__ == '__main__':
    the_gui()
    print('Exiting Program')

