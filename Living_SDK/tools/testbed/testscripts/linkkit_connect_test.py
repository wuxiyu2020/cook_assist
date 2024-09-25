import os, sys, time
from autotest import Testcase

required_devices = [ [1, 'general'] ]
MODELS_WITH_SAL = ['stm32f429']

def main():
    tc = Testcase('linkkit-connect')
    ap_ssid = tc.ap_ssid
    ap_pass = tc.ap_pass
    at = tc.at

    number, purpose = required_devices[0]
    [ret, result] = tc.start(number, purpose)
    if ret != 0:
        tc.stop('fail')
        print '\nlinkkit connect test failed'
        return [ret, result]

    responses = at.device_read_log('A', 1, 16, ['Welcome to AliOS Things'])
    if len(responses) == 0:
        tc.stop('fail')
        print '\nlinkkit connect test failed: device boot up failed'
        return [1, 'device failed to boot up']

    at.device_run_cmd('A', 'kv del hal_devinfo_pk')
    at.device_run_cmd('A', 'kv del hal_devinfo_ps')
    at.device_run_cmd('A', 'kv del hal_devinfo_dn')
    at.device_run_cmd('A', 'kv del hal_devinfo_ds')
    at.device_run_cmd('A', 'kv del wifi')
    at.device_run_cmd('A', 'reset')

    elapsed_time = time.time()
    retry = 3
    while retry > 0:
        at.device_control('A', 'reset')
        at.device_read_log('A', 1, 16, ['Welcome to AliOS Things'])
        if tc.model in MODELS_WITH_SAL: time.sleep(20) #wait for SAL finish initialize
        at.device_run_cmd('A', 'netmgr connect {0} {1}'.format(ap_ssid, ap_pass)) #connect device A to wifi
        responses = at.device_read_log('A', 1, 40, ['mqtt connect success!'])
        if len(responses) != 1:
            retry -= 1
            continue
        break
    if retry == 0:
        tc.stop('fail')
        print '\nerror: device failed to make mqtt connection, logs:'
        tc.dump_log()
        print '\nlinkkit connect test failed'
        return [1, 'failed']
    elapsed_time = time.time() - elapsed_time
    print "connected to mqtt server in {:.02f}S".format(elapsed_time)

    number_of_responses = 30
    timeout = 300
    filter = ['DM Send Message, URI:', 'Publish Result: 0']
    responses = at.device_read_log('A', number_of_responses, timeout, filter)

    ret = 0; result = 'succeed'
    if len(responses) < number_of_responses:
        print 'error: can not get {} lines of log containing {}'.format(number_of_responses, filter)
        print 'actual number of lines: {}'.format(len(responses))
        print "responses:"
        for i in range(len(responses)):
            print "  {}. {}".format(i+1, responses[i])
        ret = 1; result = 'failed'
        at.device_run_cmd('A', 'reset')
        time.sleep(2)
        tc.stop('fail')
    else:
        at.device_run_cmd('A', 'reset')
        time.sleep(2)
        tc.stop('success')

    print '\nlogs:'
    tc.dump_log()

    print '\nmqtt connect test {}'.format(result)
    return [ret, result]

if __name__ == '__main__':
    [code, msg] = main()
    sys.exit(code)
