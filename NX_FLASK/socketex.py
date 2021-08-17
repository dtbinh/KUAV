from gimbalcmd import *

pitch = 1600
roll = 1600
yaw = 1500

def dectohex(value):
    hexnum = hex(value)[2:]
    length = (len(hexnum))
    while length <= 3:
        hexnum = '0' + hexnum
        length = (len(hexnum))
    hexflip = fliphex(hexnum)
    return (hexflip)

pitch_hex = dectohex(pitch)
roll_hex = dectohex(roll)
yaw_hex = dectohex(yaw)

ser = serialinit()
safemode = responsetest(ser)
paramstore(safemode)
sleepmultipliercalc()
intervalcalc()
cmd = bytes.fromhex('FA0612' + pitch + roll + yaw + crc)
cmdexecute(cmd, sleep)
