import os, sys, time
from autotest import Testcase

required_devices = [ [1, 'general'] ]

def main(firmware='linkkit_testsuites@mk3060-general.bin', model='mk3060'):
    tc = Testcase('linkkit_testsuites')
    ap_ssid = tc.ap_ssid
    ap_pass = tc.ap_pass
    at = tc.at

    number, purpose = required_devices[0]
    [ret, result] = tc.start(number, purpose)
    if ret != 0:
        tc.stop()
        print '\nlinkkit_testsuites test failed'
        return [ret, result]

    time.sleep(16)  #wait device boot up

    at.device_run_cmd('A', 'netmgr connect {0} {1}'.format(ap_ssid, ap_pass)) #connect device A to wifi
    responses = at.device_read_log('A', 1, 1800, ['FAILED:    0'])
    if len(responses) != 1:
        at.device_run_cmd('A', 'netmgr clear')
        time.sleep(0.5)
        at.device_control('A', 'reset')
        tc.stop()
        print '\nerror: run linkkit_testsuites failed, logs:'
        tc.dump_log()
        print '\nlinkkit_testsuites test failed'
        return [1, 'failed']

    #disconnect device from alibaba cloud
    at.device_run_cmd('A', 'netmgr clear')
    time.sleep(0.5)
    at.device_control('A', 'reset')
    tc.stop()

    print '\nlogs:'
    tc.dump_log()

    print '\nlinkkit_testsuites test {}'.format(result)
    return [ret, result]

if __name__ == '__main__':
    [code, msg] = main()
    sys.exit(code)
