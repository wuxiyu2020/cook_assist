1. 运行测试程序时,需要把../ca_signals_lib.h 中的宏PRODUCTION注释掉
2. 在ca_test目录下命令行执行make
3. 在ca_test目录下命令行执行./ca_test


注意:
1. 测试相应程序时,要修改ca_test中的TEST_TYPE宏的值
2. 如果烹饪助手新增了app,需要在ca_test/Make中app变量添加相应目录配置,clean:里面添加相应删除命令