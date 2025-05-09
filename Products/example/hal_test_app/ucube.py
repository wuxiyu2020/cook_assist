src =Split('''
    linkkit_comboapp.c
    linkkit_entry.c
''')
#linkkit_sample_gateway.c
component =aos_component('linkkit_comboapp', src)

dependencis =Split('''
    framework/protocol/linkkit/sdk
    framework/protocol/linkkit/hal
    framework/netmgr
    framework/common
    framework/ywss4linkkit
    utility/cjson
    tools/cli
    framework/breeze
    framework/breeze/hal/ble
''')
for i in dependencis:
    component.add_comp_deps(i)

global_macros =Split('''
    ALIOT_DEBUG
    IOTX_DEBUG
    USE_LPTHREAD
    CONFIG_DM_DEVTYPE_SINGLE
    TEST_ALCS
    CONFIG_AOS_CLI
''')
for i in global_macros:
    component.add_global_macros(i)

if aos_global_config.get('LWIP') == 1:
    component.add_comp_deps("kernel/protocols/net")
    aos_global_config.set('no_with_lwip', 0)

if aos_global_config.get('print_heap') == 1:
    component.add_global_macros('CONFIG_PRINT_HEAP')

if aos_global_config.mcu_family == 'esp8266':
    component.add_global_macros('FOTA_RAM_LIMIT_MODE')
    component.add_global_macros('ESP8266_CHIPSET')
else :
    component.add_global_macros('CONFIG_AOS_CLI')
    component.add_comp_deps("cli")


