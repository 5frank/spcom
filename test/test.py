import os
import time
import sys
from queue import Queue
from subprocess import Popen, run, PIPE
from threading import Thread
import logging
# non std deps
import serial

logger = logging.getLogger(__name__)


SPCOM_EXE_PATH = "../spcom/build/spcom"

def logger_config(level=logging.DEBUG):
    global logger

    handler = logging.StreamHandler(sys.stderr)
    handler.setLevel(level)

    formatter = logging.Formatter("%(levelname)s:%(name)s:%(lineno)d: %(message)s")
    handler.setFormatter(formatter)

    logger.setLevel(level)
    logger.addHandler(handler)


class SpcomTestServer:
    def __init__(self, port, baudrate=115200, *args, **kwargs):
        self.ser = serial.Serial()
        self.ser.port = port
        self.ser.baudrate = baudrate
        self._read_thread = None
        self.rxq = Queue()

    def __enter__(self):
        self.connect()
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.disconnect()


    def connect(self):
        self.ser.open()
        self._read_thread = Thread(
            target=self._read_thread_worker,
            args=(self.ser, self.rxq),
            daemon=True,
        )
        self._read_thread.start()
        return 0

    def disconnect(self):
        logger.debug("closing serial port")
        self.ser.close()
        return 0

    def _read_thread_worker(self, ser, rxq):

        while ser.is_open:
            #rxline = bytearray(line)
            #logger.debug("RX: %s", rxline.hex())
            #rxq.put_nowait(line)
            data = ser.readline()
            #data = ser.read()
            logger.debug(data)
            ser.write(data)

        logger.debug("thread terminated")

def mk_spcom_cmd(port=None, baudrate=None, args=[]):

    cmd = [SPCOM_EXE_PATH,
        "--logfile", "/tmp/spcom.log" # TODO use mktempfile 
        ]

    if port is not None:
        cmd += ["--port", port]

    if baudrate is not None:
        cmd += ["--baud", str(baudrate)]

    if len(args) > 0:
        cmd += args

    return cmd

class SpcomProc:
    def __init__(self, port=None, baudrate=None, args=[]):

        cmd = mk_spcom_cmd(port, baudrate, args)
        logger.debug(cmd)

        self.rxq = Queue()
        self.p = Popen(cmd, stdin=PIPE, stdout=PIPE)
        self._read_thread = Thread(
            target=self._read_thread_worker,
            args=(self.p, self.rxq),
            daemon=True,
        )
        self._read_thread.start()

    def write(self, s, end="\n"):
        if end:
            s += end
        data = s.encode()
        self.p.stdin.write(data)

    def read(self):
        res = bytearray()
        while 1:
            data = self.rxq.get_nowait() #timeout=timeout)
            if not data:
                break
            res.extend(data)

        return res

    def _read_thread_worker(self, p, rxq):
        while 1:
            data = p.stdout.read()
            rxq.put_nowait(data)

def spcom_run(args=[]):
    cmd = [SPCOM_EXE_PATH]
    if args:
        cmd += args

    logger.debug(cmd)

    r = run(cmd, stdout=PIPE, stderr=PIPE, universal_newlines=True, check=False, shell=False)

    return r

def test_cmd(expect_success, args):
    r = spcom_run(args)
    msg = None
    if expect_success:
        if r.returncode != 0:
            msg = "exit_code {} expected zero - {}".format(r.returncode, r.args)
    else:
        if r.returncode == 0:
            msg = "exit_code {} expected non-zero - {}".format(r.returncode, r.args)

    if msg:
        logger.error(msg)


def test_simple_commands():
    test_cmd(0, args=["--this-argument-should-not-exist"])
    test_cmd(1, args=["--help"])


def main():
    """
    /dev/tnt0 <=> /dev/tnt1
    /dev/tnt2 <=> /dev/tnt3
    /dev/tnt4 <=> /dev/tnt5
    /dev/tnt6 <=> /dev/tnt7
    """
    logger_config()
    baudrate = 115200

    srvr = SpcomTestServer("/dev/tnt0", baudrate)
    srvr.connect()
    #clnt = SpcomProc("/dev/tnt1", baudrate)
    while 1:
        time.sleep(1)

    #clnt.write("hello")

main()

