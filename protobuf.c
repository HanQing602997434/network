
// C++ protobuf封包
/*
    服务器定义了封包规则：
    big endian
    head 
        2 byte body size
    body
        2 byte msgid
        2 byte datasize
        n byte data

    C++客户端(UE4)需要根据服务器定义的封包规则在发送消息时进行编码：
    完整案例如下：
    char buf[1024];
	memset(buf, 0, sizeof(buf));

	Msgtest sendMsg;
	sendMsg.set_name("xiaoming");
	sendMsg.set_age(1);
	sendMsg.set_email("xiaoming@163.com");
	sendMsg.set_online(true);
	sendMsg.set_account(888.88);

	UINT16 len = sendMsg.ByteSize();
	sendMsg.SerializeToArray(buf + 6, len);

	UINT16 bodysize = len + 4;
	UINT16 msgid = 1;
	UINT16 tlen = len;

	buf[0] = ((char*)&bodysize)[1];
	buf[1] = ((char*)&bodysize)[0];
	buf[2] = ((char*)&msgid)[1];
	buf[3] = ((char*)&msgid)[0];
	buf[4] = ((char*)&tlen)[1];
	buf[5] = ((char*)&tlen)[0];

	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);

	//向服务器发起请求
	sockaddr_in sockAddr;
	memset(&sockAddr, 0, sizeof(sockAddr)); //每个字节都用0填充
	sockAddr.sin_family = PF_INET;
	sockAddr.sin_addr.s_addr = inet_addr("192.168.0.53");
	sockAddr.sin_port = htons(8888);

	//创建套接字
	SOCKET sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	connect(sock, (SOCKADDR*)&sockAddr, sizeof(SOCKADDR));

	//获取用户输入的字符串并发送给服务器
	send(sock, buf, sendMsg.ByteSize() + 6, 0);

	//接收服务器传回的数据
	char bufRecv[128] = { 0 };
	recv(sock, bufRecv, 128, 0);
	//输出接收到的数据
	printf("Message form server: %s\n", bufRecv);

	closesocket(sock); //关闭套接字
	WSACleanup(); //终止使用 DLL

    重点：网络编码是大端编码，主机编码是小端编码
    BigEndian(大端)：低字节在高地址
    LittleEndian(小端)：低字节在低地址
    因为封包的问题既要做服务器侧又要做客户端侧做了数天，以此记录
*/