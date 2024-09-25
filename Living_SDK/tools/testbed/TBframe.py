import re, json, hashlib, logging, traceback

LOG_LEVEL = logging.DEBUG
logger = logging.getLogger(__name__)
logger.setLevel(LOG_LEVEL)
ch = logging.StreamHandler()
ch.setLevel(LOG_LEVEL)
formatter = logging.Formatter('%(name)s - %(levelname)s - %(asctime)s - %(message)s', datefmt='%Y-%m-%d %H:%M:%S')
ch.setFormatter(formatter)
logger.addHandler(ch)

def construct(packet):
    if 'id' not in packet or type(packet['id']) != int:
        logger.error('invalid id {}'.format(packet))
        return ''
    if 'command' not in packet or type(packet['command']) not in [str, unicode]:
        logger.error('invalid command {}'.format(packet))
        return ''
    if 'source' not in packet or re.match('^[0-9a-f]{12}$', packet['source']) == None:
        logger.error('invalid source {}'.format(packet))
        return ''
    if 'destination' not in packet or re.match('^[0-9a-f]{12}$', packet['destination']) == None:
        logger.error('invalid destination {}'.format(packet))
        return ''
    payload = json.dumps(packet)
    #payload = payload.encode('base64')
    if len(payload) > 99999:
        logger.error("packet size {}, too large to fit in a frame: {}".format(len(packet), packet))
        return ''
    frame = '\x02<' + '{:05d}'.format(len(payload)) + payload + '>\x03'
    return frame

def parse(buffer):
    while buffer != '':
        match = re.search('\x02<([0-9]{5})', buffer)
        if match == None:
            return {}, buffer
        length = match.group(1)
        length = int(length)
        if len(buffer) < (match.end() + length + 2):
            return {}, buffer
        if buffer[match.end() + length: match.end() + length + 2] != '>\x03':
            logger.warning('discarding bytes {}'.format(repr(buffer[:match.start() + 1])))
            buffer = buffer[match.start() + 1:]
            continue
        if match.start() > 0:
            logger.warning('discarding bytes {}'.format(repr(buffer[:match.start()])))
        payload = buffer[match.end(): match.end() + length]
        buffer = buffer[match.end() + length + 2:]
        try:
            #payload = payload.decode('base64')
            packet = json.loads(payload)
            if type(packet) != dict:
                raise ValueError
            if 'id' not in packet or type(packet['id']) != int:
                raise ValueError
            if 'command' not in packet or type(packet['command']) not in [str, unicode]:
                raise ValueError
            if 'source' not in packet or re.match('^[0-9a-f]{12}$', packet['source']) == None:
                raise ValueError
            if 'destination' not in packet or re.match('^[0-9a-f]{12}$', packet['destination']) == None:
                raise ValueError
        except:
            logger.error(traceback.format_exc())
            logger.error('discarding packet with invalid payload {}'.format(repr(payload)))
            continue
        return packet, buffer
    return {}, buffer

def hash_of_file(filename):
    h = hashlib.sha1()
    with open(filename, 'rb', buffering=0) as f:
        for b in iter(lambda : f.read(1024), b''):
            h.update(b)
    return h.hexdigest()
