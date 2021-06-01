
//Linux IO模式及select、poll、epoll详解

/*
    背景：Linux环境下的network IO
    概念说明：
        1.用户空间和内核空间
            现在的操作系统都是采用虚拟存储器，那么对32位操作系统而言，它的寻址空间（虚拟存储空间）为4G（2的32次方）。
            操作系统的核心是内核，独立于普通应用程序，可以访问受保护的内存空间，也有访问底层硬件设备的所有权限。为了
            保证用户进程不能直接操作内核（kernel），保证内核的安全，操心系统将虚拟空间划分为两部分，一部分为内核空间，
            一部分为用户空间。针对linux操作系统而言，将最高的1G字节（从0xC0000000到0xFFFFFFFF），供内核使用，称为内
            核空间，而将较低的3G字节（从虚拟地址0x00000000到0xBFFFFFFF），供各个进程使用，称为用户空间。
        2.进程切换
            为了控制进程的执行，内核必须有能力挂起正在CPU上运行的进程，并恢复以前挂起的某个进程的执行。这种行为被称为
            进程切换。因此可以说，任何进程都是在操作系统内核的支持下运行的，是与内核紧密相关的。

            从一个进程的运行转到另一个进程的运行，这个过程中经过下面这些变化：
            1.保存处理机上下文，包括程序计算器和其他寄存器
            2.更新PCB信息
            3.把进程的PCB移入相应的队列，如就绪、在某事件阻塞等队列
            4.选择另一个进程执行，并更新其PCB
            5.更新内存管理的数据结构
            6.恢复处理机上下文

            注：总而言之就是很耗资源
        3.进程的阻塞
            正在执行的进程，由于期待的某些事件未发生，如请求系统资源失败、等待某种操作的完成、新数据尚未到达或无新工作等，
            则由系统自动执行阻塞原语（Block，原语：指由若干条指令组成的程序段，用来实现某个特定功能，在执行过程中不可被中
            断），使自己由运行状态变为阻塞状态。可见进程的阻塞是进程自身的一种主动行为，也因为只有处于运行态的进程（获得
            CPU），才可能将其转为阻塞状态。当进程进入阻塞状态，是不占用CPU资源的。
        4.文件描述符
            文件描述符是计算机科学中的一个术语，是一个用于表述指向文件的引用的抽象化概念。
            文件描述符在形式上是一个非负整数。实际上，它是一个索引值，指向内核为每一个进程所维护的该进程打开文件的记录表。
            当程序打开一个现有文件或者创建一个新文件时，内核向进程返回一个文件描述符。在程序设计中，一些涉及底层的程序编写
            往往会围绕着文件描述符展开。但是文件描述符这一概念往往只适用于UNIX、Linux这样的操作系统。
        5.缓存I/O
            缓存I/O又被称作标准I/O，大多数文件系统的默认I/O操作都是缓存I/O。在Linux的缓存I/O机制中，操作系统会将I/O的数据
            缓存在文件系统的页缓存（page cache）中，也就是说，数据会被先拷贝到操作系统内核的缓冲区中，然后才会从操作系统内核
            的缓冲区拷贝到应用程序的地址空间。

            缓存I/O的缺点：
            数据在传输过程中需要在应用程序地址空间和内核进行多次数据拷贝操作，这些数据拷贝操作所带来的CPU以及内存开销是非常大的
    IO模式：
        对于一次IO访问（以read举例），数据会先被拷贝到操作系统内核的缓冲区中，然后才会从操作系统内核的缓冲区拷贝到应用程序的地址
        空间。所以说，当一个read操作发生时，它会经历两个阶段：
            1.等待数据准备（Waiting for the data to ready）
            2.将数据从内核拷贝到进程中（Copying the data from the kernel to the process）
        
        正是因为这两个阶段，Linux系统产生了下面五大网络模式的方案：
        - 阻塞I/O(blocking IO)
        - 非阻塞I/O(nonblocking IO)
        - I/O多路复用(IO multiplexing)
        - 信号驱动I/O(signal driven IO)
        - 异步I/O(asynchronous IO)

        阻塞I/O(blocking IO)
        在linux中，默认情况下所有socket都是blocking，一个典型的读操作流程大概是这样：
            当用户进程调用recvfrom这个系统调用，kernel就开始了IO的第一个阶段：准备数据（对于网络IO来说，很多时候数据在一开始还
            没有到达。比如，还没有收到一个完整的UDP包。这个时候kernel就要等待足够的数据到来）。这个过程需要等待，也就是说数据被
            拷贝到操作系统内核的缓冲区中是需要一个过程的。而在用户进程这边，整个进程会被阻塞（当然，是进程自己选择的阻塞）。当
            kernel一直等到数据准备好了，它就会将数据从kernel中拷贝到用户内存，然后kernel返回结果，用户进程才解除block的状态，
            重新运行起来。
                所以，blocking IO的特点就是在IO执行的两个阶段都被block了

        非阻塞I/O(nonblocking IO)
        linux下，可以通过设置socket使其变为non-blocking。当对一个non-blocking socket执行读操作时，流程是这个样子的：
            当用户进程发出read操作时，如果kernel中的数据没有准备好，那么它并不会block用户进程，而是立刻返回一个error。
            从用户角度讲，它发起一个read操作，并不需要等待，而是马上就得到一个结果。用户进程判断结果是一个error时，它就
            知道数据还没有准备好，于是它可以再次发送read操作。一旦kernel中的数据准备好了，并且又再次收到了用户进程的
            system call，那么它马上就将数据拷贝到了用户内存，然后返回。
                所以，nonblocking IO的特点是用户进程需要不断的主动询问kernel数据好了没有。

        I/O多路复用(IO multiplexing)
        IO multiplexing就是我们说的select，poll，epoll，有些地方也称这种IO方式为event driven IO。
            select/epoll的好处就在于单个process就可以同时处理多个网络连接的IO。它的基本原理就是select，poll，epoll这个
            function会不断的轮询所负责的所有socket，当某个socket有数据到达了，就通知用户进程。
            当用户调用了select，那么整个进程会被blocking，而同时，kernel会“监视”所有select负责的socket，当任何一个sokcet
            中的数据准备好了，select就会返回。这个时候用户进程再调用read操作，将数据从kernel拷贝到用户进程。
                所以，I/O多路复用的特点是通过一种机制一个进程能同时等待多个文件描述符，而这些文件描述符（套接字描述符）其中
                的任意一个进入就绪状态，select()函数就可以返回。

            I/O多路复用和阻塞I/O的流程图并没有太大的不同，事实上，还更差一些，因为这里需要使用两个system call（select和
            recvfrom），而blocking I/O只调用了一个system call（recvfrom）。但是，用selcet的优势在于它可以同时处理多个
            connection。

            所以，如果处理连接数不是很高的话，使用select/epoll的webserver不一定比使用multi-threading + blocking IO的
            webserver性能更好，可能延迟更大。select/epoll的优势并不是对于单个连接能处理得更快，而是在于能处理更多连接。

        异步I/O(asynchronous IO)
        Linux下的asynchronous IO其实用的很少。流程如下：
            用户进程发起read操作之后，立刻就可以开始去做其它的事。而另一方面，从kernel的角度，当它收到一个asynchronous read
            之后，首先它会立刻返回，所以不会对用户进程产生任何block。然后，kernel会等待数据准备完成，然后将数据拷贝到用户内存，
            当这一切都完成之后，kernel会给用户进程发送一个singal，告诉它read操作完成了

    I/O多路复用之selcet、poll、epoll详解
    selcet、poll、epoll都是IO多路复用的机制。I/O多路复用就是通过一种机制，一个进程可以监视多个描述符，一旦某个描述符就绪，能够
    通知程序进行相应的读写操作，但selcet、poll、epoll本质上是同步I/O，因为他们都需要在读写事件就绪后自己负责进行读写，也就是说
    这个读写过程是阻塞的，而异步I/O则无需自己负责进行读写，异步I/O的实现会负责把数据从内核拷贝到用户空间。

    select:
        select函数监视的文件描述符分3类，分别是writefds、readfds、execptfds。调用后select函数会阻塞，直到有描述符就绪（有数据
        可读、可写、或者有execpt），或者超时（timeout指定等待时间，如果立即返回设为null即可），函数返回。当select函数返回后，可
        以通过遍历fdset，来找到就绪的描述符。
        select目前几乎在所有的平台上支持，其良好跨平台支持也是它的优点。select的一个缺点在于单个进程能够监视的文件描述符数量存在
        最大限制，在Linux上一般为1024，可以通过修改宏定义甚至重新编译内核的方式提升这一限制，但是这样也会造成效率的降低。
    poll:
        不同于select使用三个位图来表示三个fdset的方式，poll使用一个pollfd的指针实现。
        pollfd结构包含了要监视的event和发生的event，不再使用select"参数-值"传递的方式。同时，pollfd并没有最大数量限制(但是数量
        过大后性能也是会下降)。和select函数一样，poll返回后，需要轮询pollfd来获取就绪的描述符。

            从上面看，select和poll都需要在返回后，通过遍历文件描述符来获取已经就绪的socket。事实上，同时连接的大量客户端在一时
            刻可能只有很少处于就绪状态，因此随着监视的描述符数量的增长，其效率也会线性下降。
    epoll:
        epoll是在2.6内核中提出来的，是之前的select和poll的增强版本。相对于select和poll来说，epoll更加灵活，没有描述符限制。
        epoll使用一个文件描述符管理多个描述符，将用户关系的文件描述符的事件存放到内核的一个事件表中，这样在用户空间和内核空间
        的copy只需一次。

        1.epoll操作过程
            epoll操作过程需要三个接口，分别如下：
                1.int epoll_create(int size);
                创建一个epoll句柄，size用来告诉内核这个监听的数目一共有多大，这个参数不同于select()中的第一个参数，给出最大
                监听的fd+1的值，参数size并不是限制了epoll所能监听的描述符最大个数，只是对内核初始分配内部数据结构的一定建议。
                
                当创建好epoll句柄后，它就会占用一个fd值，在linux下如果查看/proc/进程id/fd/，是能够看到这个fd的，所以在使用
                完epoll后，必须调用close()关闭，否则可能导致fd被耗尽。

                2.int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event);
                函数是对指定描述符fd执行op操作。
                - epfd：是epoll_create()的返回值。
                - op：表示op操作，用三个宏表示：添加EPOLL_CTL_ADD，删除EPOLL_CTL_DEL，修改EPOLL_CTL_MOD。分别添加、删除、
                修改对fd的监听事件。
                - fd：是需要监听fd（文件描述符）
                - epoll_event：是告诉内核需要监听什么事，struct epoll_event结构如下：
                
                struct epoll_event {
                    _uint32_t events; /*Epoll events
                    epoll_data_t data; /*User data variable
                };

                events可以是以下几个宏的集合；
                EPOLLIN：表示对应的文件描述符可以读（包括对端SOCKET正常关闭）
                EPOLLOUT：表示对应的文件描述符可以写
                EPOLLPRI：表示对应的文件描述符有紧急的数据可读（这里应该表示有带外数据到来）
                EPOLLERR：表示对应的文件描述符发生错误
                EPOLLHUP：表示对应的文件描述符被挂断
                EPOLLET：将EPOLL设为边沿触发（Edge Triggered）模式，这是相对于水平触发（Level Triggered）来说的
                EPOLLONESHOT：只监听一次事件，当监听完这次事件之后，如果还需要继续监听这个socket的话，需要再次把这个scoket
                              加入到EPOLL队列里
                
                3.int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout)
                等待epfd上的io事件，最多返回maxevents个事件。
                等待events用来从内核得到事件的集合，maxevents告知内核这个events有多大，这个maxevents的值不能大于创建
                epoll_create()时的size，参数timeout是超时时间（毫秒，0会立即返回，-1将不确定，也有说法是永久阻塞）。
                该函数返回需要处理的事件数目，如返回0表示已超时。
        2.工作模式
        epoll对文件描述符的操作有两种模式：LT(Level Trigger)和ET(Edge Trigger)。LT模式是默认模式，ET和LT的区别如下： 
            LT模式：当epoll_wait检测到描述符事件发生并将此事件通知应用程序，应用程序可以不立即处理该事件。下次调用epoll_wait
                    时，会再次响应应用程序并通知此事件。
            ET模式：当epoll_wait检测到描述符事件发生并将此事件通知应用程序，应用程序必须立即处理该事件。如果不处理，下次
                    调用epoll_wait时，不会再次响应应用程序并通知事件。
            1.LT模式：
                LT是缺省的工作方式，并且同时支持block和non_block socket。在这种做法中，内核告诉你一个文件描述符是否就绪了，
                然后你就可以对这个就绪的fd就行IO操作，如果你不做任何操作，内核还是会继续通知你。
            2.ET模式：
                ET是告诉工作方式，只支持non-block socket。在这个模式下，当描述符从未就绪变为就绪时，内核通过epoll告诉你。
                然后它会假设你知道文件描述符已经就绪，并且不会再为那么文件描述符发送更多的通知，直到你做了某些操作导致那个文件
                描述符不再为就绪状态了（比如，你在发送，接收或者接收请求，或者发送接收的数据少于一定量时导致一个EWOULDBLOCK
                错误）。但是请注意，如果一直不对这个fd作IO操作（从而导致它再次变成未就绪），内核不会发送更多的通知（only once）

                ET模式在很大程度上减少了epoll事件被重复触发的次数，因此效率要比LT模式高，epoll工作在ET模式的时候，必须使用非
                阻塞接口，以避免由于一个文件句柄的阻塞读/阻塞写操作把处理多个文件描述符的任务饿死。
            3.总结：
                假设有这个一个例子：
                1.我们已经把一个用来从管道中读取数据的文件句柄(RFD)添加到epoll描述符
                2.这个时候从管道的另一端被写入了2KB的数据
                3.调用epoll_wait(2)，并且它会返回RFD，说明它已经准备好读取操作
                4.然后我们读取了1KB的数据
                5.调用epoll_wait(2)......

                LT模式：
                    如果是LT模式，那么在第5步调用epoll_wait(2)之后，仍然能收到通知。
                
                ET模式：
                    如果我们在第1步将RFD添加到epoll描述符的时候使用EPOLLET标志，那么在第5步调用epoll_wait(2)之后将有可能挂起，
                    因为剩余的数据还存在于文件的缓冲区中，而且数据发出端还在等待一个针对已经发出数据的反馈信息。只有监视的文件句柄
                    上发生了某个事件时ET工作模式才会汇报事件。因此在第5步的时候，调用者可能会放弃等待仍在存在于文件输入缓冲区的剩
                    余数据。

                    当使用epoll的ET模式来工作时，当产生一个EPOLLIN事件后，读数据的时候需要考虑的是当recv()返回的大小如果等于请求的
                    大小，那么很有可能是缓冲区还有数据未读完，也意味着该次事件还没有处理完，所以还需要再次读取：

                    while (rs) {
                        buflen = recv(activeevents[i].data.fd, buf, sizeof(buf), 0);
                        if (buflen < 0) {
                            //由于是非阻塞模式，所以当errno为EAGAIN时，表示当前缓冲区已无数据可读
                            //在这里就当作是该次事件已处理
                            if (errno == EAGAIN) {
                                break;
                            } else {
                                return;
                            }
                        } else if (buflen == 0) {
                            //这里表示对端的socket已正常关闭
                        }

                        if (buflen == sizeof(buf)) {
                            rs = 1; //需要再次读取
                        } else {
                            rs = 0；
                        }
                    }

                    Linux中的EAGAIN含义：
                    Linux环境下开发经常会碰到很多错误(设置errno)，其中EAGAIN是其中比较常见的一个错误（比如用在非阻塞操作中）。
                    从字面上来看，是提示再试一次。这个错误经常出现在当应用程序进行一些非阻塞(non-blocking)操作（对文件或socket）
                    的时候。
                    例如，以O_NONBLOCK的标志打开文件/socket/FIFO，如果你连续做read操作而没有数据可读。此时程序不会阻塞起来等待
                    数据准备就绪返回，read函数会返回一个错误EAGAIN，提示你的应用程序现在没有数据可读请稍后再试。
                    又例如，当一个系统调用（比如fork）因为没有足够的资源（比如虚拟内存）而执行失败，返回EAGAIN提示其再调用一次(也
                    许下次就能成功)。
        3.代码演示最下方
    4.epoll总结：
        在select/epoll中，进程只有在调用一定的方法后，内核才对所有监视的文件描述符进行扫描，而epoll事件通过epoll_ctl()来注册一个
        文件描述符，一旦基于某个文件描述符就绪时，内核会采用类似callback的回调机制，迅速激活这个文件描述符，当进程调用epoll_wait()
        便得到通知。（此处去掉了遍历文件描述符，而是通过监听回调的机制。这正是epoll的魅力所在。）
            epoll的优点主要是以下几个方面：
            1.监视的描述符数量不受限制，它所支持的fd上限是最大可以打开文件的数目，这个数字远大于2048，举个例子，在1GB内存的机器上大约
            是10万左右，具体数目可以cat/proc/sys/fs/file-max查看，一般来说这个数目和内存关系很大。select的最大缺点就是进程打开的fd
            是有数量限制的。这对于连接数量比较大的服务器来说根本不能满足。虽然也可以选择多进程的解决方案（Apache就是这样实现的），不过
            虽然linux上面创建进程的代价比较小，但仍旧不可忽视的，加上进程间数据同步远比不上线程间同步的高效，所以也不是一种完美的方案。
            2.IO的效率不会随着监视fd的数量的增长而下降。epoll不同于select和poll轮询的方式，而是通过每个fd定义的回调函数来实现的，只有
            就绪的fd才会执行回调函数。
            3.如果没有遇到大量的idle-connection或者dead-connection，epoll的效率并不会比select/poll高很多，但是当遇到大量的
            idle_connection，就会发现epoll的效率大大高于select/poll。
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <errno.h>
#include <sys/epoll.h> 

int main(int argc, char *argv[]) {
    if (argc < 2) {
        return -1;
    }
    //传入端口号
    int port = atoi(argv[1]);
    //创建listen fd
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if((sockfd < 0)) {
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(struct sockaddr_in));

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    //绑定listen fd
    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(struct sockaddr_in))) {
        return -2;
    }

    if (listen(sockfd, 5) < 0) {
        return -3;
    }
    //创建一个管理的fd
    int epfd = epoll_create(1);
    
    //对于events数组的大小，对于IO密集型可以做到总数量的1/100
    struct epoll_event ev, events[1024] = {0};
    ev.events = EPOLLIN;
    ev.data.fd = sockfd;
    //将监听fd加入红黑树
    epoll_ctl(epfd, EPOLL_CTL_ADD, sockfd, &ev);

    while (1) {
        //等待数据，事件个数返回给nready
        int nready = epoll_wait(epfd, events, 1024, -1);
        if (nready < -1) {
            break;
        }

        int i = 0;
        for (i = 0; i < nready; ++i) {
            
            //socket分为两类：1.accept() 2.recv()/send()
            if (events[i].data.fd == sockfd) {

                struct sockaddr_in client_addr;
                memset(&client_addr, 0, sizeof(struct sockaddr_in));
                socklen_t client_len = sizeof(client_addr);

                //只有一个socket是监听的，其他全部都是accept返回的
                int clientfd = accept(sockfd, (struct sockaddr*)&client_addr, &client_len);
                if (clientfd <= 0) continue;

                char str[INET_ADDRSTRLEN] = {0};
                printf("recv from %s at port %d\n", inet_ntop(AF_INET, &client_addr.sin_addr, str, sizeof(str)), ntohs(client_addr.sin_port));

                ev.events = EPOLLIN | EPOLLET;
                ev.data.fd = clientfd;
                //将accept出来的socket
                epoll_ctl(epfd, EPOLL_CTL_ADD, clientfd, &ev);
            } else {
                int clientfd = events[i].data.fd;

                char buffer[1024] = {0};
                int ret = recv(clientfd, buffer, 1024, 0);
                if (ret < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        printf("EAGAIN\n");
                    } else {

                    }
                    close(clientfd);

                    ev.events = EPOLLIN;
                    ev.data.fd = clientfd;
                    epoll_ctl(epfd, EPOLL_CTL_DEL, clientfd, &ev);

                } else if (ret == 0) {
                    printf("disconnect %d\n", clientfd);
                    close(clientfd);

                    ev.events = EPOLLIN;
                    ev.data.fd = clientfd;
                    epoll_ctl(epfd, EPOLL_CTL_DEL, clientfd, &ev);
                } else {

                    printf("Recv：%s, %d Bytes\n", buffer, ret);

                }
            }

        }
    }
}