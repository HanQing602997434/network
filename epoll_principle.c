
//epoll实现原理

/*
一、epoll数据结构
    epoll主要由两个结构体：eventpoll与epitem。epitem是每个IO所对应的事件。比如epoll_ctl EPOLL_CTL_ADD操作
    的时候，就需要创建一个epitem。epollevent是每一个epoll所对应的。比如epoll_create就是创建一个eventpoll。
    
    epitem的定义：
    struct epitem {
        RB_ENTRY(epitem) rbn;
        LIST_ENTRY(epitem) rdlink;
        int rdy; //exist in list

        int sockfd;
        struct epoll_event event;
    };

    eventpoll的定义：
    struct eventpoll {
        ep_rb_tree rbr;
        LIST_HEAD( ,epitem) rdlist;
        int rdnum;

        int waiting;

        pthread_mutex_t mtx; //rbtree update
        pthread_spinlock_t lock; //rdlist update

        pthread_cond_t cond; //block for event
        pthread_mutex_t cdmtx; //mutex for cond
    };

    list用来存储准备就绪的IO，对于数据结构主要讨论两方面：insert与remove。何时将数据插入到list中呢？当内核IO
    准备就绪的时候，会执行epoll_event_callback的回调函数，将epitem添加到list中。
    那何时删除list中的元素呢？当epoll_wait激活重新运行的时候，将list的epitem逐一copy到events参数中。

    rbtree用来存储所有IO的数据，方便快速通过IO_fd查找，也从insert与remove讨论。对于rbtree何时添加：当app执行
    epoll_ctl EPOLL_CTL_ADD操作，将epitem添加到rbtree中。

    list和rbtree的操作又如何做到线程安全，SMP，防止死锁呢？

二、epoll锁机制
    epoll从以下几个方面是需要加锁保护的。list操作，rbtree操作，epoll_wait的等待。
    list使用最小粒度的锁spinlock，便于在SMP下添加操作的时候，能够快速操作list.
    list添加：
        pthread_spin_lock(&ep->lock);  //获取spinlock
        epi->rdy = 1;  //epitem的rdy置为1，代表epitem已经在就绪队列中，后续再触发相同事件就只需更改event
        LIST_INSERT_HEAD(&ep->rdlist, epi, rdlink);  //添加到list中
        ep->rdnum ++;  //将eventpoll的rdnum 域 加1
        pthread_spin_unlock(&ep->lock);  //释放spinlock
    list删除：
        pthread_spin_lock(&ep->lock);  //获取spinlock

        int cnt = 0;
        int num = (ep->rdnum > maxevents ? maxevents : ep->rdnum);  //判读rdnum与maxevents的大小，避免event溢出
        int i = 0;

        while (num != 0 && !LIST_EMPTY(&ep->rdlist)) {  //循环遍历list，判断添加的list不能为空

            struct epitem *epi = LIST_FIRST(&ep->rdlist);  //获取list首个节点
            LIST_REMOVE(epi, rdlink);  //移除list首个节点
            epi->rdy = 0;  //将epitem的rdy域置为0，标识epitem不再就绪队列中

            memcpy(&events[i++], &epi->event, sizeof(struct epoll_event));  copy epitem的event到用户空间events

            num --;
            cnt ++;  //copy数量加1
            cp->rdnum --;  //eventpoll中rdnum减1
        }

        pthread_spin_unlock(&ep->lock);
        避免SMP体系下，多核竞争。此处采用自旋锁，不适合采用睡眠锁

    rbtree添加：
        pthread_mutex_lock(&ep->mtx);  //获取互斥锁

        struct epitem tmp;
        tmp.sockfd = sockid;
        struct epitem *epi = RB_FIND(_epoll_rb_socket, &ep->rbr, &tmp);  //查找sockid的epitem是否存在，存在则不能添加
        if (epi) {
            nty_trace_epoll("rbtree is exist\n");
            pthread_mutex_unlock(&ep->mtx);
            return -1;
        }

        epi = (struct epitem*)calloc(1, sizeof(struct epitem));  //分配epitem空间
        if (!epi) {
            pthread_mutex_unlock(&ep->mtx);
            errno = -ENOMEM;
            return -1;
        }

        epi->sockfd = sockid;  //sockid赋值
        memcpy(&epi->event, event, sizeof(struct epoll_event));  //将设置的event添加到epi的event域

        epi = RB_INSERT(_epoll_rb_socket, &ep->rbr, epi);  //将epitem添加到rbrtree
        assert(epi == NULL);

        pthread_mutex_unlock(&ep->mtx);  //释放互斥锁

    rbtree删除：
        pthread_mutex_lock(&ep->mtx);  //获取互斥锁

        struct epitem tmp;
        tmp.sockfd = sockid;
        struct epitem *epi = RB_REMOVE(_epoll_rb_socket, &ep->rbr, &tmp);  //删除sockid的节点，不存在返回-1
        if (!epi) {
            nty_trace_epoll("rbtree is no exist\n");
            pthread_mutex_unlock(&ep->mtx);
            return -1;
        }   

        free(epi);  //释放epitem 

        pthread_mutex_unlock(&ep->mtx);  //释放互斥锁

三、epoll回调
    epoll的回调函数何时执行，此部分需要与Tcp的协议栈一起来阐述。Tcp协议栈的时序图如下图所示，epoll从协议栈回调的
    部分有以下1，2，3，4来阐述：
        1.tcp三次握手，对端反馈ack后，socket进入revd状态。需要将监听socket的event置为EPOLLIN，此时标识可以进入
        到accept读取socket数据。
        2.在established状态，收到数据之后，需要将socket的event置为EPOLLIN状态。
        3.在established状态，收到fin时，此时socket进入到close_wait。需要socket的event置为EPOLLIN。读取断开信息
        4.检测socket的send状态，如果对端cwnd>0是可以发送的数据。故需要将socket置为EPOLLOUT。
        所以在此四处添加epoll的回调函数，即可使得epoll正常接收到io事件。

四、LT和ET
    LT（水平触发）和ET（边沿触发）是电子信号里面的概念。
    ET在event发生改变时调用epoll回调函数，LT查看event是否为EPOLLIN，则可调用epoll回调函数。
*/