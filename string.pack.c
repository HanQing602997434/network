
//lua string.pack string.unpack
/*
    在了解string.pack string.unpack之前，先了解一下大端编码和小端编码

    大端字节序(网络字节序) 小端字节序(主机字节序)
    大端就是将高位字节放到内存的低地址端，地位字节放到高地址端，地位字节放到高地址端。网络传输中(比如TCP/IP)低地址端(高字节端)放在流的开始。
    对于2个字节的字符串(ab)，传输顺序为：a(0-7bit)、b(8-15bit)。
    之所以又成为网络字节序，是因为网路传输时，默认是大端编码传输的。
    如果把计算机的内存看作是一个很大的字节数组，一个字节包含8bit信息可以表示0-255的无符号整型，以及-128-127的有符号整型。
    当存储一个大于8bit的值到内存时，这个值常常会被切分成多个8bit的segment存储在一个连续的内存空间中，一个segment一个字节。
    有些处理器会把高位存储在内存这个字节数组的头部，把地位存储在尾部，这种处理方式叫大端字节序，有些处理器则相反，低位存储
    在头部，高位存储在尾部，称之为小端字节序。

    lua string.pack string.unpack
    函数pack负责将不同的变量打包在一起，成为一个字节字符串。
    函数unpack将字节字符串解包成为变量。

    字节符b打包解包pack("b", str) unpck("b", str)：
        local unpack = string.unpack
        local pack = string.pack
        local str1 = pack(">b", -128) -- 最小支持 -128
        local str2 = pack("<b", 127)  -- 最大支持 127

        -- 如果把pack("<b", 127) 改为 pack("b", 128)就会出现下面的错误
        -- bad argument #2 to 'pack' (integer overflow)，pack的第二个参数整型溢出了
        print(unpack(">b", str1)) -- 输出 -128 2 ，这个2表示下一个字节的位置
        print(unpack("<b", str2)) -- 输出 127 2 ， 这个2表示下一个字节的位置

    字节符B打包解包pack("B", str) unpack("B", str)：
        local str1 = pack("B", 0)   -- 最小支持0
        local str2 = pack("B", 255) -- 最大支持255

        -- 如果改为 pack("B", -1) 或者 pack("B", 256)，就会出现下面的错误
        -- bad argument #2 to 'pack' (unsigned overflow)，pack第二个参数类型溢出了
        print(unpack("B", str1)) -- 输出 0 2 ， 这个2表示下一个字节的位置
        print(unpack("B", str2)) -- 输出 255 2，这个2表示下一个字节的位置

    字节符H打包解包pack("H", str) unpack("H", str)：
        -- H表示unsigned short
        local str1 = pack("H", 0)     -- 最小支持 0
        local str2 = pack("H", 65535) -- 最大支持 65535
        -- 如果改为pack("H", -1) 或者 pack("H", 65536)，就会出现下面的错误
        -- bad argument #2 to 'pack' (unsigned overflow)，意思是pack的第二个参数类型溢出了
        print(unpack("H", str1)) -- 输出 0 3，这个3表示下一个short的位置，每个short占2字节
        print(unpack("H", str2)) -- 输出65535 3，这个3表示下一个short的位置，每个short占2字节

    字节符h打包解包pack("h", str) unpack("h", str)：
        local str1 = pack("h", -32768) -- 最小支持 -32768
        local str2 = pack("h", 32767)  -- 最大支持 32767
        -- 如果改为pack("h", -32769) 或者 pack("h", 32768)，就会出现下面的错误
        -- bad argument #2 to 'pack' (integer overflow)，意思是pack的第二个参数类型溢出了
        print(unpack("h", str1)) 输出-32768 3，这个3表示下一个short的位置，每个short占2个字节
        print(unpack("h", str2)) 输出32767 3，这个3表示下一个short的位置

    字节符I打包解包pack("I", str) unpack("I", str)：
        -- I默认是占4字节，但是可以给I指定字节数，如I2就是占2字节，I3就是占3字节
        local str1 = pack("I", 0)          -- 最小支持0
        local str2 = pack("I", 4294967295) -- 最大支持4294967295
        -- 如果改为pack("I", -1) 或者 pack("I", 4294967296)，就会出现下面的错误
        -- bad argument #2 to 'pack' (unsigned overflow)，意思是pack的第二个参数类型溢出了
        print(unpack("I", str1)) -- 输出 0 5，这个5表示下一个字节的位置，I默认占4字节
        print(unpack("I", str2)) -- 输出 4294967295 5，这个5表示下一个字节的位置

        local str3 = pack("I2", 0)     -- 最小支持0
        local str4 = pack("I2", 65535) -- 最大支持65535
        print(unpack("I2", str3)) -- 输出 0 3，这个3表示下一个字节的位置，I2占2字节
        print(unpack("I2", str4)) -- 输出 65535 3，这个3表示下一个字节的位置

        local str5 = pack("I3", 0)        -- 最小支持0
        local str6 = pack("I3", 16777215) -- 最大支持16777215
        print(unpack("I3", str5)) -- 输出0 4，这个4表示下一个字节的位置，I3占3个字节
        print(unpack("I3", str6)) -- 输出16777215 4，这个4表示下一个字节的位置

    字节符c打包解包pack("c", str) unpack("c", str)：
        -- c表示一个字节
        local str1 = pack("c", 'a') -- 表示最大支持1个字节的字符
        print('#str1', #str1) --输出'#str1' 1
        -- 如果改为pack("c", 'aa')，就会出现如下的错误
        -- bad argument #2 to 'pack' (string longer than given size)
        local str2 = pack("c5", 1) -- 表示最大支持5个字节的字符
        print("#str2", #str2) -- 输出#str2 5

        print(unpack("c1", str1)) -- 输出a 2，这个2表示下一个字节的位置，c1占1字节
        print(unpack("c5", str2)) -- 输出1 6，这个6表示下一个字节的位置，c5占5字符

    字符s打包解包pack("s", str) unpack("s", str)
        -- s，可指定头部占用字节数，默认8字节
        local temp1 = pack("s", 'a')
        print(unpack("s", temp1)) -- 输出a 10，这个10表示下一个字符的位置

        local temp2 = pack("s", 'abc')
        print(unpack("s", temp2)) -- 输出abc 12，这个表示下一个字符的位置

        local temp3 = pack("ss", 'abc', 'efg')
        print(unpack("s", temp3, 12)) -- 输出efg 23，这个23表示下一个字符的位置

        -- s默认头部占8字节，s1表示头部占1字节、s2表示头部占2字节

        local temp4 = pack("s2", "abc")
        print(unpack("s2", temp4)) -- 输出abc 6，这个6表示下一个字符的位置

        local temp5 = pack("s2s2", 'abc', 'efg')
        print(unpack("s2", temp5, 6)) -- 输出efg 11，这个11表示下一个字符的位置
*/