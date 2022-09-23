#!/usr/bin/env python3
import usb.core
import usb.util
import struct
import sys
from pathlib import Path

def main(fn):
    dev = usb.core.find(idVendor=0x057E, idProduct=0x3000)
    if dev is None:
        print('usb device not found')
        return

    dev.set_configuration()
    cfg = dev.get_active_configuration()
    intf = cfg.interfaces()[0]

    def ep_in_matcher(ep):
        return usb.util.endpoint_direction(ep.bEndpointAddress) == usb.util.ENDPOINT_IN

    def ep_out_matcher(ep):
        return usb.util.endpoint_direction(ep.bEndpointAddress) == usb.util.ENDPOINT_OUT

    epin = usb.util.find_descriptor(intf, custom_match=ep_in_matcher)
    epout = usb.util.find_descriptor(intf, custom_match=ep_out_matcher)

    MAGIC = 0x54555452  # TUTR
    CHUNKSIZE = 0x1000000

    def wait_ack():
        resp = epin.read(16, timeout=0)
        magic, type_, _, _ = struct.unpack('IIII', resp)
        assert magic == MAGIC and type_ == 3

    fn = Path(fn)
    if not fn.exists():
        print(f'{fn} not exists')
        return
    filename = fn.name.encode('utf-8')
    # send length of filename
    epout.write(struct.pack('IIII', MAGIC, 1, 0, len(filename)), timeout=0)
    wait_ack()
    # send filename
    epout.write(filename, timeout=0)
    wait_ack()
    # loop
    tot = fn.stat().st_size
    pos = 0
    f = fn.open('rb')
    while True:
        # send range
        bs = f.read(CHUNKSIZE)
        epout.write(struct.pack('IIII', MAGIC, 2, pos, pos + len(bs)), timeout=0)
        print(f'send range {pos}-{pos + len(bs)}')
        wait_ack()
        if pos == tot:
            break
        pos += len(bs)
        # send data
        epout.write(bs, timeout=0)
        wait_ack()
    f.close()
    print('done')


if len(sys.argv) == 2:
    main(sys.argv[1])
else:
    print('Usage: usblink.py PATH_TO_FILE')
