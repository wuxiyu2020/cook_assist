del /a /f /s /q "*.ini"
del /a /f /s /q "iot.toml"
del /a /f /s /q "iot_linux.toml"
del /a /f /s /q "mcu.toml"
del /a /f /s /q "storage.toml"
del /a /f /s /q "partition.bin"
del /a /f /s /q "ro_params.dtb"
del /a /f /s /q .\chips\tg7100c\efuse_bootheader\*.bin
del /a /f /s /q .\chips\tg7100c\img_create_iot\*.bin
del /a /f /s /q .\chips\tg7100c\img_create_iot\*.pack
del /a /f /s /q .\chips\tg7100c\img_create_mcu\*.bin
del /a /f /s /q .\chips\tg7100c\img_create_mcu\*.pack
rd /s /Q .\chips\tg7100c\ota
del /a /f /s /q .\chips\tg6210a\efuse_bootheader\*.bin
del /a /f /s /q .\chips\tg6210a\img_create_iot\*.bin
del /a /f /s /q .\chips\tg6210a\img_create_iot\*.pack
del /a /f /s /q .\chips\tg6210a\img_create_mcu\*.bin
del /a /f /s /q .\chips\tg6210a\img_create_mcu\*.pack
rd /s /Q .\chips\tg6210a\ota
rd /s /Q .\chips\tg6210a\test_bin
del /a /f /s /q ".DS_Store"
del /a /f /s /q "._.DS_Store"
del /a /f /s /q "md_test_privatekey_ecc.pem"
del /a /f /s /q "md_test_publickey_ecc.pem"
pause