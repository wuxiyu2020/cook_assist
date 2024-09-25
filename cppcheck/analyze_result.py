
# -*- coding: utf-8 -*-

# 解释器：python2

import codecs


def main():
    fp = codecs.open("./result.txt", "r", encoding="utf-8")
    lines = fp.read()
    lines = lines.split('\n')
    fp.close()

    new_fp = codecs.open("./result.txt", "w+", encoding="utf-8")
    i = 0
    while True:
        if i == len(lines) - 1 or len(lines) < 2:
            break

        # 把函数声明中，形参数组越界的误判过滤掉  如：
        #   uint8_t mac[6];
        #   extern int bl_efuse_read_mac_factory(uint8_t mac[6]);  形参index使用6没问题，不应该报错
        if "arrayIndexOutOfBounds" in lines[i] and "extern" in lines[i+1]:
            i += 3
            continue
        else:
            # 把其他类型的错误写到原文件里
            new_fp.write(lines[i] + '\n')
            i += 1
    
    # 过滤后，如果./result.txt文件中还有数据，说明存在其他问题，return 1
    new_fp.seek(0, 0)
    content = new_fp.read()
    new_fp.close()
    if content:
        print(content)
        return 1
    
    return 0


if __name__ == "__main__":
    exit(main())