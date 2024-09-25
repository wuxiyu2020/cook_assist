import os, sys, time
from autotest import Testcase

required_devices = [ [1, 'general'] ]
MODELS_WITH_SAL = ['stm32f429']

def main():
    tc = Testcase('mqtt-connect')
    ap_ssid = tc.ap_ssid
    ap_pass = tc.ap_pass
    at = tc.at

    number, purpose = required_devices[0]
    [ret, result] = tc.start(number, purpose)
    if ret != 0:
        print '\nmqtt connect test failed'
        tc.stop('fail')
        return [ret, result]

    responses = at.device_read_log('A', 1, 16, ['Welcome to AliOS Things'])
    if len(responses) == 0:
        print '\nmqtt connect test failed: device boot up failed'
        tc.stop('fail')
        return [1, 'device failed to boot up']

    at.device_run_cmd('A', 'kv del hal_devinfo_pk')
    at.device_run_cmd('A', 'kv del hal_devinfo_ps')
    at.device_run_cmd('A', 'kv del hal_devinfo_dn')
    at.device_run_cmd('A', 'kv del hal_devinfo_ds')
    at.device_run_cmd('A', 'kv del wifi')

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
        print '\nmqtt connect test failed'
        return [1, 'failed']
    elapsed_time = time.time() - elapsed_time
    print "connected to mqtt server in {:.02f}S".format(elapsed_time)

    number_of_responses = 30
    timeout = 300
    filter = ['Upstream Topic:', 'Upstream Payload:', 'publish message:', 'publish topic msg=', 'publish success']
    responses = at.device_read_log('A', number_of_responses, timeout, filter)

    ret = 0; result = 'succeed'
    if len(responses) < number_of_responses:
        print 'error: can not get {} lines of log containing {}'.format(number_of_responses, filter)
        print 'actual number of lines: {}'.format(len(responses))
        print "responses:"
        for i in range(len(responses)):
            print "  {}. {}".format(i+1, responses[i])
        ret = 1; result = 'failed'
        tc.stop('fail')
    else:
        tc.stop('success')

    print '\nlogs:'
    tc.dump_log()

    print '\nmqtt connect test {}'.format(result)
    return [ret, result]

if __name__ == '__main__':
    [code, msg] = main()
    sys.exit(code)
