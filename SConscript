from building import *

cwd = GetCurrentDir()
src = Split("""
src/chat.c
src/modem.c
src/pppnetif.c
""")

if GetDepend('MODEM_TYPE_M6312'):
    src += ['src/m6312.c']

CPPPATH = [cwd + '/inc']
group = DefineGroup('SerialModem', src, depend = ['PKG_USING_SERIALMODEM'], CPPPATH = CPPPATH)

Return('group')
