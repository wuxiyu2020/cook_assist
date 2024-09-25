#!/usr/bin/python

import os, sys, time, socket, ssl, traceback, logging
import subprocess, thread, threading, Queue, random, re, errno
from os import path
import TBframe as pkt

MAX_MSG_LENGTH = 65536
ENCRYPT = True
DEBUG = True
LOG_LEVEL = logging.DEBUG

SUPPORTED_MODELS = {
    'atsame54p20': {'address': '0x0'},
    'atsaml21j18' : {'address':'0x0'},
    'bk7231s': {'address': '0x11000'},
    'bk7231u': {'address': '0x11000'},
    'csky': {'address': '0x10010800'},
    'developerkit': {'address': '0x8000000'},
    'esp32': {'address': '0x10000'},
    'esp8266': {'address':  '0x1000'},
    'gd32f307vc': {'address': '0x8000000'},
    'gd32f350rb': {'address': '0x8000000'},
    'gd32f450zk': {'address': '0x8000000'},
    'lpc54018': {'address': '0x10000000'},
    'lpc54102': {'address': '0x0'},
    'lpc54114': {'address': '0x0'},
    'lpc54628': {'address': '0x0'},
    'mimxrt1052xxxxb': {'address': '0x60080000'},
    'mk3060': {'address': '0x13200'},
    'mk3070': {'address': '0x40000'},
    'mk3072': {'address': '0xa000'},
    'mk3080': {'address': '0x0'},
    'nrf52832': {'address': '0x0'},
    'nrf52832_xxaa': {'address': '0x0'},
    'rda5981': {'address': '0x4000'},
    'stm32f091': {'address': '0x8000000'},
    'stm32f103': {'address': '0x8000000'},
    'stm32f429': {'address': '0x8000000'},
    'stm32l432': {'address': '0x8000000'},
    'stm32l475': {'address': '0x8000000'},
    'starterkit': {'address': '0x8000000'},
    'sv6266': {'address': '0x0'},
    'w600': {'address': '0x0'},
    'xr871': {'address': '0x0'},
}

AVAILABLE_WIFIS = [
    {'ssid':'aos_test_01', 'password': 'Alios@Embedded'},
    {'ssid':'alibaba-test-1227981', 'password': 'Alios@Things'},
    {'ssid':'alibaba-test-1227982', 'password': 'Alios@Things'},
]

class ConnectionLost(Exception):
    pass

class Autotest:
    def __init__(self):
        self.keep_running = True
        self.device_list= {}
        self.service_socket = None
        self.connected = False
        self.output_queue = Queue.Queue(256)
        self.response_queue = Queue.Queue(256)
        self.sequence = 0
        self.uuid = os.urandom(6).encode('hex')
        self.subscribed = {}
        self.subscribed_reverse = {}
        self.filter = {}
        self.filter_lock = threading.RLock()
        self.filter_event = threading.Event()
        self.filter_event.set()
        self.esc_seq = re.compile(r'\x1b[^m]*m')
        self.log_buffer = ['None', 'invaid', time.time() - 0.5]
        self.poll_str = '\x1b[t' + os.urandom(4).encode('hex') + 'm'
        self.logger = logging.getLogger(__name__)
        self.logger.setLevel(LOG_LEVEL)
        ch = logging.StreamHandler()
        ch.setLevel(LOG_LEVEL)
        formatter = logging.Formatter('%(levelname)s - %(asctime)s - %(message)s', datefmt='%Y-%m-%d %H:%M:%S')
        ch.setFormatter(formatter)
        self.logger.addHandler(ch)
        random.seed()

    def construct_packet(self, content):
        packet = {}
        packet['id'] = self.sequence
        self.sequence += 1
        packet.update(content)
        packet['source'] = self.uuid
        if 'destination' not in packet:
            packet['destination'] = '000000000000'
        return packet

    def packet_send_thread(self):
        heartbeat_timeout = time.time() + 10
        while self.keep_running:
            try:
                packet = self.output_queue.get(block=True, timeout=0.1)
            except Queue.Empty:
                packet = None
                pass
            if self.service_socket == None:
                continue
            if packet == None:
                if time.time() < heartbeat_timeout:
                    continue
                heartbeat_timeout += 10
                content = {'command': 'HEARTBEAT'}
                data = pkt.construct(self.construct_packet(content))
            else:
                data = pkt.construct(packet)
            try:
                self.service_socket.send(data)
            except:
                self.connected = False
                continue
        #self.logger.info("packet send thread exited")

    def send_packet(self, packet, timeout=0.1):
        if self.service_socket == None:
            return False
        try:
            self.output_queue.put(packet, True, timeout)
            return True
        except Queue.Full:
            self.logger.warning('output buffer full, drop packet {}'.format(packet))
        return False

    def get_devname_by_device(self, device):
        if device in list(self.subscribed_reverse):
            return self.subscribed_reverse[device]
        return ""

    def get_device_by_devname(self, devname):
        if devname in list(self.subscribed):
            return self.subscribed[devname]['device']
        return ""

    def response_filter(self, devname, logstr):
        if self.filter_event.is_set() == True:
            return

        if self.filter_lock.acquire(False) == False:
            return

        try:
            if len(self.filter) == 0:
                self.filter_lock.release()
                return

            if self.filter['devname'] != devname:
                self.filter_lock.release()
                return

            if self.filter['cmdstr'] != self.poll_str and self.poll_str not in logstr:
                self.filter_lock.release()
                return

            logstr = logstr.replace(self.poll_str, '')

            if self.filter['lines_exp'] == 0:
                if self.filter['cmdstr'] in logstr:
                    self.filter['lines_num'] += 1
                    self.filter_event.set()
            else:
                if self.filter['lines_num'] == 0:
                    if self.filter['cmdstr'] in logstr:
                        self.filter['lines_num'] += 1
                elif self.filter['lines_num'] <= self.filter['lines_exp']:
                    log = self.esc_seq.sub('', logstr)
                    log = log.replace("\r", "")
                    log = log.replace("\n", "")
                    if log != "":
                        for filterstr in self.filter['filters']:
                            if filterstr not in log:
                                continue
                            else:
                                self.filter['response'].append(log)
                                self.filter['lines_num'] += 1
                                break
                    if self.filter['lines_num'] > self.filter['lines_exp']:
                        self.filter_event.set()
            self.filter_lock.release()
        except:
            if DEBUG: traceback.print_exc()
            self.filter_lock.release()

    def connect_to_server(self, server_ip, server_port):
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        if ENCRYPT:
            certfile = path.join(path.dirname(path.abspath(__file__)), 'certificate.pem')
            sock = ssl.wrap_socket(sock, cert_reqs=ssl.CERT_REQUIRED, ca_certs=certfile)
        try:
            sock.connect((server_ip, server_port))
        except:
            return "fail"

        self.service_socket = sock
        self.service_socket.settimeout(2)
        content = {'command': "LOGIN", 'role': 'testscript'}
        self.send_packet(self.construct_packet(content))
        try:
            data = sock.recv(MAX_MSG_LENGTH)
            packet, data = pkt.parse(data)
            if 'command' not in packet or packet['command'] != 'LOGIN_RESPONSE' or \
                'result' not in packet or packet['result'] != 'success':
                self.logger.info('{}'.format(packet))
                raise ValueError
        except:
            self.service_socket = None
            sock.shutdown(socket.SHUT_RDWR)
            sock.close()
            return 'fail'
        self.service_socket.settimeout(None)
        self.connected = True
        return "success"

    def server_interaction(self):
        buffer = ''
        devices_need_resubscribe = False
        while self.keep_running:
            try:
                if self.connected == False:
                    raise ConnectionLost

                new = self.service_socket.recv(MAX_MSG_LENGTH)
                if new == '':
                    if self.keep_running: raise ConnectionLost
                    break

                buffer += new
                while buffer != '':
                    packet, buffer = pkt.parse(buffer)
                    if packet == {}:
                        break

                    command = packet['command']
                    if command == "DEVICE_LIST":
                        devices = packet['devices']
                        new_list = {}
                        for device in devices:
                            try:
                                [uuid, port] = device['name'].split('+')
                            except:
                                continue
                            if uuid != device['hostid']:
                                continue
                            device['port'] = port
                            new_list[device['name']] = device

                        for dev in list(self.device_list):
                            if dev not in list(new_list):
                                self.device_list.pop(dev)
                        self.device_list = new_list

                        #device resubscribe processing
                        if devices_need_resubscribe == False:
                            continue
                        for devname in self.subscribed:
                            if self.subscribed[devname]['subscribed'] == True:
                                continue
                            if self.subscribed[devname]['device'] not in new_list:
                                continue
                            device = self.subscribed[devname]['device']
                            content = {'command': "SUBSCRIBE", 'device': device, 'target': 'all'}
                            self.send_packet(self.construct_packet(content))
                            self.subscribed[devname]['subscribed'] = True
                        devices_need_resubscribe= False
                        for devname in self.subscribed:
                            if self.subscribed[devname]['subscribed'] == True:
                                continue
                            devices_need_resubscribe= True
                            break

                        continue
                    if command == "DEVICE_LOG":
                        device = packet['device']
                        logtime = packet['logtime']
                        logdata = packet['logdata'].decode('base64')
                        try:
                            logtimestr = time.strftime("%Y-%m-%d %H-%M-%S", time.localtime(logtime));
                            logtimestr += ("{:.3f}".format(logtime-int(logtime)))[1:]
                        except:
                            continue
                        if device not in list(self.device_list):
                            continue
                        devname = self.get_devname_by_device(device)
                        if devname != "":
                            if self.poll_str not in logdata: self.log_buffer= [devname, logdata, time.time()]
                            self.response_filter(devname, logdata)
                            if self.logfile != None:
                                self.logfile.write('{}:{}:'.format(logtimestr, devname))
                                self.logfile.write(logdata)
                        continue
                    if command.endswith("_RESPONSE"):
                        try:
                            self.response_queue.put(packet, False)
                        except:
                            pass
                        continue
            except ConnectionLost:
                if self.service_socket != None:
                    self.service_socket.close()
                    self.service_socket = None
                    self.logger.warning('connection to server lost')
                    try:
                        time.sleep(0.5 + random.random())
                    except KeyboardInterrupt:
                        break
                    self.logger.info('try reconnecting...')

                self.connected = False
                result = self.connect_to_server(self.server_ip, self.server_port)
                if result != 'success':
                    self.logger.info('connect to server failed, retry later ...')
                    time.sleep(2)
                    continue
                self.logger.info('connection to server resumed')

                for devname in self.subscribed:
                    device = self.subscribed[devname]['subscribed'] = False
                devices_need_resubscribe = True
            except socket.error as e:
                if e.errno == errno.ECONNRESET:
                    self.logger.warning('connection closed by server')
                    self.connected = False
                    continue
                if DEBUG:traceback.print_exc()
                break
            except:
                if DEBUG: traceback.print_exc()
                break
        self.keep_running = False
        #self.logger.info("server interaction thread exited")

    def excute_command(self, content, timeout):
        packet = self.construct_packet(content)
        while self.response_queue.empty() == False:
            self.response_queue.get()
        result = 'timeout'
        message = 'timeout'
        self.send_packet(packet)
        start = time.time()
        while True:
            try:
                response = self.response_queue.get(False)
            except:
                response = None
            if time.time() - start >= timeout:
                break
            if response == None:
                time.sleep(0.01)
                continue
            if response['command'] == packet['command'] + '_RESPONSE' and \
               response['id'] == packet['id'] and response['source'] == packet['destination']:
                result = response['result']
                message = response['message']
                break
        return result, message

    def send_file_to_client(self, devname, filepath):
        #argument check
        if devname not in self.subscribed:
            self.logger.error("device {} is not subscribed".format(devname))
            return False
        device = self.subscribed[devname]['device']
        if device not in self.device_list:
            self.logger.error("device {} does not exist in device list".format(device))
            return False
        destination = self.device_list[device]['hostid']
        try:
            expandfilepath = path.expanduser(filepath)
        except:
            self.logger.error("file {} does not exist".format(filepath))
            return False
        if path.exists(expandfilepath) == False:
            self.logger.error("file {} does not exist".format(filepath))
            return False
        filepath = expandfilepath

        filehash = pkt.hash_of_file(filepath)

        #send file begin
        content = {
            'command': 'FILE_TRANSFER',
            'destination': destination,
            'device': device,
            'operate': 'begin',
            'filehash': filehash,
            'filename': path.basename(filepath),
            }
        retry = 4
        while retry > 0:
            result, message = self.excute_command(content, 0.3)
            if result == 'timeout':
                retry -= 1;
                continue
            if result == 'busy':
                self.logger.info("file transfer busy: wait...")
                time.sleep(5)
                continue
            elif result == 'success' or result == 'exist':
                break
            else:
                self.logger.error("file transfer error: unexpected result '{}'".format(result))
                return False
        if retry == 0:
            self.logger.info('file transfer error: retry timeout')
            return False

        if result == 'exist':
            return True

        #send file data
        seq = 0
        file = open(filepath,'r')
        data = file.read(4096)
        while(data):
            content = {
                'command': 'FILE_TRANSFER',
                'destination': destination,
                'device': device,
                'operate': 'data',
                'filehash': filehash,
                'sequence': seq,
                'filedata': data.encode('base64'),
                }
            retry = 4
            while retry > 0:
                result, message = self.excute_command(content, 0.3)
                if result == 'timeout':
                    retry -= 1;
                    continue
                if result != 'success':
                    print('send data fragement {} failed, error: {}'.format(seq, message))
                    file.close()
                    return False
                break

            if retry == 0:
                file.close()
                return False

            seq += 1
            data = file.read(4096)
        file.close()

        #send file end
        content = {
            'command': 'FILE_TRANSFER',
            'destination': destination,
            'device': device,
            'operate': 'end',
            'filehash': filehash,
            'filename': path.basename(filepath),
            }
        retry = 4
        while retry > 0:
            result, message = self.excute_command(content, 0.3)
            if result == 'timeout': #timeout
                retry -= 1;
                continue
            if result != 'success':
                print('send file failed, error: {}'.format(message))
                return False
            break
        if retry == 0:
            return False
        return True

    def device_allocate(self, model, number, timeout, purpose='general', reserve='yes'):
        """ request server to allocte free/ilde devices to do autotest

        arguments:
           model  : device model, example: 'mk3060', 'esp32'
           number : number of devices to allocate
           timeout: time (seconds) to wait before timeout
           purpose: specify test purpose, example: 'alink'

        return: allocated device list; empty list if failed
        """
        if self.connected == False:
            return []
        content = {
            "command": "DEVICE_ALLOCATE",
            "model": model,
            "number": number,
            "purpose": purpose,
            "reserve": reserve,
            }
        allocated = []
        timeout += time.time()
        while time.time() < timeout:
            result, allocated = self.excute_command(content, 0.8)
            if result != 'success': #failed
                if allocated == 'unavailable':
                    return 'unavailable'
                allocated = 'busy'
                time.sleep(8)
                continue
            if type(allocated) != list:
                self.logger.error('illegal allocate reponse - {}'.format(allocated))
                allocated = 'error'
                break
            if len(allocated) != number:
                self.logger.error("allocated number does not equal requested")
                allocated = 'error'
            break
        return allocated

    def device_subscribe(self, devices):
        """ unsubscribe to devices, thus start receiving log from these devices

        arguments:
           devices : the list of devices to subscribe

        return: Ture - succeed; False - failed
        """
        if self.connected == False:
            return False
        for devname in list(devices):
            device = devices[devname]
            if device == "":
                self.subscribed = {}
                self.subscribed_reverse = {}
                return False
            content = {'command': "SUBSCRIBE", 'device': device, 'target': 'log'}
            result, message = self.excute_command(content, 0.2)
            if result != 'success': return False
            self.subscribed[devname] = {'device': device, 'subscribed': True};
            self.subscribed_reverse[device] = devname
        return True

    def device_unsubscribe(self, devices):
        """ unsubscribe to devices, thus stop receiving log from these devices

        arguments:
           devices : the list of devices to unsubscribe

        return: Ture - succeed; False - failed
        """
        for devname in list(devices):
            if devname not in self.subscribed:
                continue
            device = devices[devname]
            content = {'command': "UNSUBSCRIBE", 'device': device, 'target': 'all'}
            result, message = self.excute_command(content, 0.2)
            if result != 'success': return False
            self.subscribed_reverse.pop(device)
            self.subscribed.pop(devname)
        return True

    def device_erase(self, devname):
        """ erase device

        arguments:
           devname : the device to be erased

        return: Ture - succeed; False - failed
        """
        if self.connected == False:
            return False
        if devname not in self.subscribed:
            self.logger.error("device {} is not subscribed".format(devname))
            return False
        device = self.subscribed[devname]['device']
        if device not in self.device_list:
            self.logger.error("device {} does not exist in device list".format(device))
            return False
        content = {
            'command': 'DEVICE_OPERATE',
            'destination': self.device_list[device]['hostid'],
            'device': device,
            'operate': 'erase',
            }
        result, message = self.excute_command(content, 120)
        return (result == "success")

    def device_program(self, devname, address, filepath):
        """ program device with a firmware file

        arguments:
           devname : the device to be programmed
           address : start address write the firware(in hex format), example: '0x13200'
           filepath: firmware file path, only bin file is supported right now

        return: Ture - succeed; False - failed
        """
        if self.connected == False:
            return False
        if devname not in self.subscribed:
            self.logger.error("device {} is not subscribed".format(devname))
            return False
        device = self.subscribed[devname]['device']
        if device not in self.device_list:
            self.logger.error("device {} does not exist in device list".format(device))
            return False
        destination = self.device_list[device]['hostid']
        if address.startswith('0x') == False:
            self.logger.error("wrong address input {}, address should start with 0x".format(address))
            return False
        try:
            expandname = path.expanduser(filepath)
        except:
            self.logger.error("file {} does not exist".format(filepath))
            return False
        if path.exists(expandname) == False:
            self.logger.error("file {} does not exist".format(filepath))
            return False

        retry = 3
        while retry:
            if self.send_file_to_client(devname, expandname) == False:
                retry -= 1
                time.sleep(5)
            else:
                break
        if retry == 0:
            return False

        filehash = pkt.hash_of_file(expandname)
        content = {
            'command': 'DEVICE_OPERATE',
            'destination': destination,
            'device': device,
            'operate': 'program',
            'address': address,
            'filehash': filehash,
            }
        elapsed_time = time.time()
        result, message = self.excute_command(content, 270)
        elapsed_time = time.time() - elapsed_time
        self.logger.info("Programming device {} finished in {:0.1f}S, result: {}".format(devname, elapsed_time, message))
        return (result == "success")

    def device_control(self, devname, operation):
        """ control device

        arguments:
           operation : the control operation, which can be 'start', 'stop' or 'reset'
                       note: some device may not support 'start'/'stop' operation

        return: Ture - succeed; False - failed
        """
        operations = ["start", "stop", "reset"]
        if self.connected == False:
            return False
        if devname not in self.subscribed:
            return False
        device = self.subscribed[devname]['device']
        if device not in self.device_list:
            self.logger.error("device {} does not exist in device list".format(device))
            return False
        destination = self.device_list[device]['hostid']
        if operation not in operations:
            return False

        content = {
            'command': 'DEVICE_OPERATE',
            'destination': destination,
            'device': device,
            'operate': operation,
            }
        result, message = self.excute_command(content, 10)
        return (result == 'success')

    def device_run_cmd(self, devname, args, response_lines = 0, timeout=0.8, filters=[""]):
        """ run command at device

        arguments:
           devname       : the device to run command
           args          : command arguments, example: 'netmgr connect wifi_ssid wifi_password'
           response_lines: number of response lines to wait for, default to 0 (do not wait response)
           timeout       : time (seconds) before wait reponses timeout, default to 0.8S
           filter        : filter applied to response, only response line containing filter item will be returned
                           by default, filter=[""] will match any response line

        return: list containing filtered responses
        """

        if self.connected == False:
            return False
        if devname not in self.subscribed:
            return False
        device = self.subscribed[devname]['device']
        if device not in self.device_list:
            self.logger.error("device {} does not exist in device list".format(device))
            return False
        destination = self.device_list[device]['hostid']
        if len(args) == 0:
            return False
        content = {
            'command': "DEVICE_OPERATE",
            'destination': destination,
            'device': device,
            'operate': 'runcmd',
            'args': self.poll_str + args,
            }
        with self.filter_lock:
            self.filter['devname'] = devname
            self.filter['cmdstr'] = args
            self.filter['lines_exp'] = response_lines
            self.filter['lines_num'] = 0
            self.filter['filters'] = filters
            self.filter['response'] = []

        retry = 3
        while retry > 0:
            self.send_packet(self.construct_packet(content))
            self.filter_event.clear()
            self.filter_event.wait(timeout)
            if self.filter['lines_num'] > 0:
                break;
            retry -= 1
        response = self.filter['response']
        with self.filter_lock:
            self.filter = {}
        return response

    def device_read_log(self, devname, lines, timeout=1, filters=[""]):
        """ read device log ouput

        arguments:
           devname       : the device to read output from
           response_lines: number of response lines to read
           timeout       : time (seconds) to wait before timeout, default to 1S
           filter        : filter applied to response, only log line containing filter item will be returned
                           by default, filter=[""] will match any response line

        return: list containing log lines
        """

        if self.connected == False:
            return False
        if devname not in self.subscribed:
            self.logger.error("device {} is not subscribed".format(devname))
            return False
        if lines <= 0:
            return False
        with self.filter_lock:
            self.filter['devname'] = devname
            self.filter['cmdstr'] = self.poll_str
            self.filter['lines_exp'] = lines
            self.filter['lines_num'] = 1
            self.filter['filters'] = filters
            self.filter['response'] = []

        self.filter_event.clear()

        # try read log from log buffer
        [devname, logstr, logtime] = self.log_buffer
        now = time.time()
        #print self.log_buffer, now - logtime
        if (now - logtime) < 0.06:
            self.response_filter(devname, logstr)

        self.filter_event.wait(timeout)
        response = self.filter['response']
        with self.filter_lock:
            self.filter = {}
        return response

    def start(self, server_ip, server_port, logname=None):
        """ start Autotest moudle, establish connnection to server

        arguments:
           server_ip  : server ip/hostname to connect
           server_port: server TCP port number to connect
           logname    : log file name(log saved at ./test/logname); logname=None means do not save log

        return: Ture - succeed; False - failed
        """

        self.keep_running = True
        thread.start_new_thread(self.packet_send_thread, ())
        self.server_ip = server_ip
        self.server_port = server_port
        result = self.connect_to_server(server_ip, server_port)
        if result != 'success':
            self.logger.error("connect to server {}:{} failed".format(server_ip, server_port))
            return False
        thread.start_new_thread(self.server_interaction, ())

        self.logfile = None
        if logname != None:
            if path.exists('testlog') == False:
                subprocess.call(['mkdir','testlog'])

            try:
                self.logfile = open("testlog/"+logname, 'w');
            except:
                self.logger.error("open logfile {} failed".format(logfile))
                return False

        time.sleep(0.5)
        return True

    def stop(self, result='success'):
        """ stop Autotest moudle, disconnect server and clean up

        return: Ture - succeed; False - failed
        """

        if self.service_socket and len(self.subscribed_reverse) and result != 'success':
            content = {
                'command': 'TEST_FAILED',
                'devices': list(self.subscribed_reverse),
                }
            self.send_packet(self.construct_packet(content))
            time.sleep(0.1)
        self.keep_running = False
        if self.service_socket:
            try:
                self.connected = False
                self.service_socket.shutdown(socket.SHUT_RDWR)
                self.service_socket.close()
            except:
                pass
        time.sleep(0.2)
        if hasattr(self, 'logfile') and self.logfile != None:
            self.logfile.close()
            self.logfile = None

class Testcase:
    server = '11.238.148.13'
    port = 16384
    model = 'unknown'
    firmware = None
    at=Autotest()

    def __init__(self, testname='default', argv=None):
        if argv == None:
            argv = sys.argv
        random.seed()
        wifi_ap = AVAILABLE_WIFIS[int(random.random() * len(AVAILABLE_WIFIS))]
        self.ap_ssid = wifi_ap['ssid']
        self.ap_pass = wifi_ap['password']
        self.parse_args(argv)

        now=time.strftime('%Y-%m-%d_%H-%M-%S')
        randbytes = os.urandom(4).encode('hex')
        self.logname = '{}@{}@{}_{}.log'.format(testname, self.model, now, randbytes)

    def parse_args(self, argv):
        #parse input
        i = 1
        while i < len(argv):
            arg = sys.argv[i]
            if arg.startswith('--firmware='):
                args = arg.split('=')
                if len(args) != 2:
                    at.logger.info('wrong argument {} input, example: --firmware=firmware.bin'.format(arg))
                self.firmware = args[1]
            elif arg.startswith('--model='):
                args = arg.split('=')
                if len(args) != 2:
                    at.logger.info('wrong argument {} input, example: --model=mk3060'.format(arg))
                self.model = args[1]
            elif arg.startswith('--server='):
                args = arg.split('=')
                if len(args) != 2:
                    at.logger.info('wrong argument {} input, example: --server=192.168.10.16'.format(arg))
                    return [1, 'argument {} error'.format(arg)]
                self.server = args[1]
            elif arg.startswith('--port='):
                args = arg.split('=')
                if len(args) != 2 or args[1].isdigit() == False:
                    at.logger.info('wrong argument {} input, example: --port=16384'.format(arg))
                    return [1, 'argument {} error'.format(arg)]
                self.port = int(args[1])
            elif arg.startswith('--wifissid='):
                args = arg.split('=')
                if len(args) != 2:
                    at.logger.info('wrong argument {} input, example: --wifissid=test_wifi'.format(arg))
                self.ap_ssid = args[1]
            elif arg.startswith('--wifipass='):
                args = arg.split('=')
                if len(args) != 2:
                    at.logger.info('wrong argument {} input, example: --wifipass=test_password'.format(arg))
                self.ap_pass = args[1]
            elif arg.startswith('--times='):
                args = arg.split('=')
                if len(args) != 2:
                    print 'wrong argument {} input, example: --times=100'.format(arg)
                self.times = args[1]
            elif arg.startswith('--duration='):
                args = arg.split('=')
                if len(args) != 2:
                    print 'wrong argument {} input, example: --duration=48'.format(arg)
                self.duration = args[1]
            elif arg.startswith('--deviceName='):
                args = arg.split('=')
                if len(args) != 2:
                    print 'wrong argument {} input, example: --deviceName=dn'.format(arg)
                self.deviceName = args[1]
            elif arg.startswith('--userId='):
                args = arg.split('=')
                if len(args) != 2:
                    print 'wrong argument {} input, example: --userId=id'.format(arg)
                self.userId = args[1]
            elif arg.startswith('--frequency='):
                args = arg.split('=')
                if len(args) != 2:
                    print 'wrong argument {} input, example: --frequency=2'.format(arg)
                self.frequency = args[1]
            elif arg=='--help':
                at.logger.info('Usage: python {} [--firmware=xxx.bin] [--wifissid=wifi_ssid] [--wifipass=password]'.format(sys.argv[0]))
                return [0, 'help']
            else:
                if "=" in arg:
                    args = arg.split('=')
                    key = args[0].replace("--", "")
                    self.__dict__[key] = args[1]
                else:
                    print("wrong argument {} input, example: --key=value")

            i += 1

    def start(self, number, purpose):
        at = self.at
        model = self.model
        server = self.server
        port = self.port
        firmware = self.firmware

        if model not in SUPPORTED_MODELS:
            at.logger.error("unsupported model {}".format(repr(model)))
            return [1, 'unsupported model']

        if at.start(server, port, self.logname) == False:
            return [1, 'connect testbed failed']

        #request device allocation
        timeout = 10
        allocated = at.device_allocate(model, number, timeout, purpose)
        if type(allocated) != list or len(allocated) != number:
            return [1, 'allocate device failed']
        devices={} #construct device list
        for i in range(len(allocated)):
            devices[chr(ord('A') + i)] = allocated[i]
        at.logger.info('allocated:')
        for devname in sorted(list(devices)):
            device = devices[devname]
            port = at.device_list[device]['port']
            hostname = at.device_list[device]['hostname']
            at.logger.info('    {}: {} @ {}'.format(devname, port, hostname))

        #subscribe
        result = at.device_subscribe(devices)
        if result == False:
            at.logger.error('subscribe devices failed')
            return [1, 'subscribe devices failed']

        #program devices
        device_list = sorted(list(devices))
        address = SUPPORTED_MODELS[model]['address']
        index = 0
        while index < len(device_list):
            device = device_list[index]
            at.device_erase(device)
            result = at.device_program(device, address, firmware)
            self.app_start_time = time.time()
            if result == False:
                at.logger.info('program failed, try allocating another...')
                another = at.device_allocate(model, 1, 60, purpose)
                if type(another) != list or len(another) != 1:
                    at.logger.info('try allocate another failed')
                    return [1, 'program failed and allocate another failed']
                at.device_unsubscribe({device: devices[device]})
                at.device_subscribe({device: another[0]})
                devices[device] = another[0]
                at.logger.info('allocated another {}: {}'.format(device, devices[device]))
                continue
            index += 1
        at.logger.info('program succeed')
        return [0, 'succeed']

    def stop(self, result='success'):
        self.at.stop(result)

    def get_model(self):
        return self.model

    def get_firmware(self):
        return self.firmware

    def get_log(self):
        flog = open('testlog/' + self.logname, 'r')
        return flog

    def dump_log(self):
        flog = open('testlog/' + self.logname, 'r')
        for line in flog:
            try:
                logtime = line.split(':')[0]
                sec_int, sec_frac = logtime.split('.')
                sec_int = time.mktime(time.strptime(sec_int, "%Y-%m-%d %H-%M-%S"))
                sec_frac = float('0.' + sec_frac)
                logtime = sec_int + sec_frac
                if logtime >= self.app_start_time:
                    print line,
            except:
                print line,
        flog.close()
