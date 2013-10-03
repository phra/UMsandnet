/*   This is part of um-ViewOS
 *   The user-mode implementation of OSVIEW -- A Process with a View
 *
 *   UMNET: (Multi) Networking in User Space
 *   Copyright (C) 2008  Renzo Davoli <renzo@cs.unibo.it>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License, version 2, as
 *   published by the Free Software Foundation.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License along
 *   with this program; if not, write to the Free Software Foundation, Inc.,
 *   51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <dlfcn.h>
#include <time.h>
#include <pthread.h>
#include <sys/mount.h>
#include <linux/net.h>
#include <sys/utsname.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <poll.h>
//#include "config.h"
#include "module.h"
#include "libummod.h"
#include "test.h"

#define S_IFSTACK 0160000
#define SOCK_DEFAULT 0

#define TRUE 1
#define FALSE 0
#define printf printk

//#define DEFAULT_NET_PATH "/dev/net/default"

#ifndef __UMNET_DEBUG_LEVEL__
#define __UMNET_DEBUG_LEVEL__ 0
#endif

#ifdef __UMNET_DEBUG__
#define PRINTDEBUG(level,args...) printdebug(level, __FILE__, __LINE__, __func__, args)
#else
#define PRINTDEBUG(level,args...)
#endif

#define BUFSTDIN 16
#define PATHLEN 256
#define MAX_FD 128
#define WHITE 1
#define BLACK 2

struct ht_elem* htuname,* htfork,* htvfork,* htclone,* htopen,* htsocket,* htread,* htwrite;
char connections[MAX_FD];

#define puliscipuntatore(p) memset(p,0,sizeof(*p))
#define puliscistruct(s) memset(&s,0,sizeof(s))
#define pulisciarray(a) memset(a,0,sizeof(a))
#define likely(x) __builtin_expect((x),1)
#define unlikely(x) __builtin_expect((x),0)


static struct service s;
VIEWOS_SERVICE(s)
/*
struct umnet
{
    char *path;
    int pathlen;
    void *dlhandle;
    struct umnet_operations *netops;
    unsigned long flags;
    long mode;
    uid_t uid;
    gid_t gid;
    time_t mounttime;
    time_t sockettime;
    void *private_data;
    struct ht_elem *socket_ht;
};

struct fileinfo
{
    int nfd;
    struct umnet *umnet;
};
#if 0
#define WORDLEN sizeof(int *)
#define WORDALIGN(X) (((X) + WORDLEN) & ~(WORDLEN-1))
#define SIZEDIRENT64NONAME (sizeof(__u64)+sizeof(__s64)+sizeof(unsigned short)+sizeof(unsigned char))
#endif

struct umnetdefault
{
    int count;
    struct umnet *defstack[AF_MAXMAX];
};

static struct umnetdefault **defnet=NULL;
/ TAG for NULLNET /
#define NULLNET ((struct umnet*)defnet)
static int defnetsize=0;

void *net_getdl(struct umnet *mh)
{
    return mh->dlhandle;
}

static long umnet_addproc(int id, int ppid, int max)
{
    int size=max+1;
    if (size > defnetsize)
    {
        struct umnetdefault **newdefnet;
        newdefnet = realloc(defnet,size*sizeof(struct umnetdefault *));
        if (newdefnet == NULL)
            return -1;
        else
        {
            for (; defnetsize<size; defnetsize++)
                newdefnet[defnetsize]=NULL;
            defnet=newdefnet;
        }
    }
    if (id == ppid)
    {
        //printk("defnet ROOT %d\n",id);
        defnet[id]=NULL;
    }
    else
    {
        //printk("+net %d<-%d %p %p\n",id,ppid,defnet[ppid],defnet[ppid]);
        defnet[id]=defnet[ppid];
        if (defnet[id] != NULL)
        {
            //printk("+net %d<-%p %x %d\n",id,defnet[id],defnet[id]->count);
            defnet[id]->count++;
        }
    }
    return 0;
}

static long umnet_delproc(int id)
{
    if (defnet[id] != NULL)
    {
        //printk("-net %d %p %d\n",id,defnet[id],defnet[id]->count);
        if (defnet[id]->count <= 0)
            free(defnet[id]);
        else
            defnet[id]->count--;
        defnet[id]=NULL;
    }
    return 0;
}

static void umnet_delallproc(void)
{
    int i;
    for(i=0; i<defnetsize; i++)
        umnet_delproc(i);
    free(defnet);
}

static long umnet_setdefstack(int id, int domain, struct umnet *defstack)
{
    if (domain > 0 && domain < AF_MAXMAX)
    {
        //printk("umnet_setdefstack %d %d %p\n",id,domain,defstack);
        if (defnet[id] == NULL)
            defnet[id] = calloc(1,sizeof (struct umnetdefault));
        if (defnet[id] != NULL)
        {
            if (defnet[id]->defstack[domain-1] != defstack)
            {
                if (defnet[id]->count > 0)
                {
                    struct umnetdefault *new=malloc(sizeof (struct umnetdefault));
                    if (new)
                    {
                        memcpy(new,defnet[id],sizeof (struct umnetdefault));
                        new->count=0;
                        defnet[id]->count--;
                        defnet[id]=new;
                    }
                    else
                    {
                        errno=EINVAL;
                        return -1;
                    }
                }
                defnet[id]->defstack[domain-1] = defstack;
            }
            return 0;
        }
        else
        {
            errno=EINVAL;
            return -1;
        }
    }
    else
    {
        errno=EINVAL;
        return -1;
    }
}

static struct umnet *umnet_getdefstack(int id, int domain)
{
    if (domain > 0 && domain <= AF_MAXMAX && defnet[id] != NULL)
    {
        //printk("umnet_getdefstack %d %d\n",id,domain);
        //printk("   %p %p\n",defnet[id],defnet[id]->defstack[domain-1]);
        return defnet[id]->defstack[domain-1];
    }
    else
    {
        struct ht_elem *hte=ht_search(CHECKPATH,DEFAULT_NET_PATH,
                                      strlen(DEFAULT_NET_PATH),&s);
        if (hte)
            return ht_get_private_data(hte);
        else
            return NULL;
    }
}

static long umnet_ctl(int type, char *sender, va_list ap)
{
    int id, ppid, max;

    switch(type)
    {
    case MC_PROC | MC_ADD:
        id = va_arg(ap, int);
        ppid = va_arg(ap, int);
        max = va_arg(ap, int);
        /*printk("umnet_addproc %d %d %d\n",id,ppid,max);/
        return umnet_addproc(id, ppid, max);

    case MC_PROC | MC_REM:
        id = va_arg(ap, int);
        /*printk("umnet_delproc %d\n",id);/
        return umnet_delproc(id);

    default:
        return -1;
    }
}

static long umnet_ioctlparms(int fd,int req)
{
    //printk("fd %d arg %d\n",fd,req);
    struct fileinfo *ft=getfiletab(fd);

    if(ft->umnet->netops->ioctlparms)
    {
        return ft->umnet->netops->ioctlparms(
                   ft->nfd, req, ft->umnet);
    }
    else
    {
        return 0;
    }
}

static int checksocket(int type, void *arg, int arglen,
                       struct ht_elem *ht)
{
    int *family=arg;
    struct umnet *mc=umnet_getdefstack(um_mod_getumpid(),*family);
    //printk("checksocket %d %d %p\n",um_mod_getumpid(),*family,mc);
    if (mc==NULL)
    {
        char *defnetstr=ht_get_private_data(ht);
        if (defnetstr)
            return defnetstr[*family];
        else
            return 0;
    }
    else
    {
        return 1;
    }
}

static long umnet_msocket(char *path, int domain, int type, int protocol)
{
    struct umnet *mh;
    long rv;
    printk("MSOCKET PORCODIO.");
    if (path)
        mh = um_mod_get_private_data();
    else
        mh = umnet_getdefstack(um_mod_getumpid(),domain);
    if (mh == NULL)
    {
        errno = EAFNOSUPPORT;
        return -1;
    }
    //printk("msocket %s %d %d %d\n",path,domain, type, protocol);
    if (type == SOCK_DEFAULT)
    {
        if (domain == PF_UNSPEC)
        {
            for (domain=1; domain<=AF_MAXMAX; domain++)
                if (!mh->netops->supported_domain ||
                        mh->netops->supported_domain(domain))
                    umnet_setdefstack(um_mod_getumpid(),domain,mh);
            return 0;
        }
        else
        {
            return umnet_setdefstack(um_mod_getumpid(),domain,mh);
        }
    }
    else if (mh->netops->msocket)
    {
        rv=mh->netops->msocket(domain, type, protocol, mh);
        if (rv >= 0)
        {
            int fd = addfiletab(sizeof(struct fileinfo));
            struct fileinfo *ft=getfiletab(fd);
            ft->nfd = rv;
            ft->umnet = mh;
            rv=fd;
            mh->sockettime=time(NULL);

        }
        return rv;
    }
    else
    {
        errno = EINVAL;
        return -1;
    }
}

static long umnet_bind(int fd, const struct sockaddr *addr,
                       socklen_t addrlen)
{
    struct fileinfo *ft=getfiletab(fd);
    if(ft->umnet->netops->bind)
    {
        return ft->umnet->netops->bind(
                   ft->nfd, addr, addrlen);
    }
    else
    {
        errno = EINVAL;
        return -1;
    }
}

static long umnet_connect(int fd, const struct sockaddr *serv_addr,
                          socklen_t addrlen)
{
    struct fileinfo *ft=getfiletab(fd);
    if(ft->umnet->netops->connect)
    {
        return ft->umnet->netops->connect(
                   ft->nfd, serv_addr, addrlen);
    }
    else
    {
        errno = EINVAL;
        return -1;
    }
}

static long umnet_listen(int fd, int backlog)
{
    struct fileinfo *ft=getfiletab(fd);
    if(ft->umnet->netops->listen)
    {
        return ft->umnet->netops->listen(
                   ft->nfd, backlog);
    }
    else
    {
        errno = EINVAL;
        return -1;
    }
}

static long umnet_accept(int fd, struct sockaddr *addr, socklen_t *addrlen)
{
    struct fileinfo *ft=getfiletab(fd);
    if(ft->umnet->netops->accept)
    {
        long rv;
        rv=ft->umnet->netops->accept(
               ft->nfd, addr, addrlen);
        if (rv >= 0)
        {
            int fd2 = addfiletab(sizeof(struct fileinfo));
            struct fileinfo *ft2=getfiletab(fd2);
            ft2->nfd = rv;
            ft2->umnet = ft->umnet;
            rv=fd2;
        }
        return rv;
    }
    else
    {
        errno = EINVAL;
        return -1;
    }
}

static long umnet_getsockname(int fd, struct sockaddr *name, socklen_t *namelen)
{
    struct fileinfo *ft=getfiletab(fd);
    if(ft->umnet->netops->getsockname)
    {
        return ft->umnet->netops->getsockname(
                   ft->nfd, name, namelen);
    }
    else
    {
        errno = EINVAL;
        return -1;
    }
}

static long umnet_getpeername(int fd, struct sockaddr *name, socklen_t *namelen)
{
    struct fileinfo *ft=getfiletab(fd);
    if(ft->umnet->netops->getpeername)
    {
        return ft->umnet->netops->getpeername(
                   ft->nfd, name, namelen);
    }
    else
    {
        errno = EINVAL;
        return -1;
    }
}

static long umnet_send(int fd, const void *buf, size_t len, int flags)
{
    struct fileinfo *ft=getfiletab(fd);
    if(ft->umnet->netops->send)
    {
        return ft->umnet->netops->send(
                   ft->nfd, buf, len, flags);
    }
    else
    {
        errno = EINVAL;
        return -1;
    }
}

static long umnet_recv(int fd, void *buf, size_t len, int flags)
{
    struct fileinfo *ft=getfiletab(fd);
    if(ft->umnet->netops->recv)
    {
        return ft->umnet->netops->recv(
                   ft->nfd, buf, len, flags);
    }
    else
    {
        errno = EINVAL;
        return -1;
    }
}

static long umnet_sendto(int fd, const void *buf, size_t len, int flags,
                         const struct sockaddr *to, socklen_t tolen)
{
    struct fileinfo *ft=getfiletab(fd);
    if(ft->umnet->netops->sendto)
    {
        return ft->umnet->netops->sendto(
                   ft->nfd, buf, len, flags, to, tolen);
    }
    else
    {
        errno = EINVAL;
        return -1;
    }
}

static long umnet_recvfrom(int fd, void *buf, size_t len, int flags,
                           struct sockaddr *from, socklen_t *fromlen)
{
    struct fileinfo *ft=getfiletab(fd);
    if(ft->umnet->netops->recvfrom)
    {
        return ft->umnet->netops->recvfrom(
                   ft->nfd, buf, len, flags, from, fromlen);
    }
    else
    {
        errno = EINVAL;
        return -1;
    }
}

long umnet_sendmsg(int fd, const struct msghdr *msg, int flags)
{
    struct fileinfo *ft=getfiletab(fd);
    if (ft->umnet->netops->sendmsg)
        return(ft->umnet->netops->sendmsg(ft->nfd,msg,flags));
    else
        return umnet_sendto(ft->nfd,msg->msg_iov->iov_base,msg->msg_iov->iov_len,flags,
                            msg->msg_name,msg->msg_namelen);
}

long umnet_recvmsg(int fd, struct msghdr *msg, int flags)
{
    struct fileinfo *ft=getfiletab(fd);
    if (ft->umnet->netops->recvmsg)
        return(ft->umnet->netops->recvmsg(ft->nfd, msg, flags));
    else
    {
        msg->msg_controllen=0;
        return umnet_recvfrom(ft->nfd,msg->msg_iov->iov_base,msg->msg_iov->iov_len,flags,
                              msg->msg_name,&msg->msg_namelen);
    }
}

static long umnet_getsockopt(int fd, int level, int optname,
                             void *optval, socklen_t *optlen)
{
    struct fileinfo *ft=getfiletab(fd);
    if(ft->umnet->netops->getsockopt)
    {
        return ft->umnet->netops->getsockopt(
                   ft->nfd, level, optname, optval, optlen);
    }
    else
    {
        errno = EINVAL;
        return -1;
    }
}

static long umnet_setsockopt(int fd, int level, int optname,
                             const void *optval, socklen_t optlen)
{
    struct fileinfo *ft=getfiletab(fd);
    if(ft->umnet->netops->setsockopt)
    {
        return ft->umnet->netops->setsockopt(
                   ft->nfd, level, optname, optval, optlen);
    }
    else
    {
        errno = EINVAL;
        return -1;
    }
}

static long umnet_read(int fd, void *buf, size_t count)
{
    struct fileinfo *ft=getfiletab(fd);
    if(ft->umnet->netops->read)
    {
        return ft->umnet->netops->read(
                   ft->nfd, buf, count);
    }
    else
    {
        errno = EINVAL;
        return -1;
    }
}

static long umnet_write(int fd, const void *buf, size_t count)
{
    struct fileinfo *ft=getfiletab(fd);
    if(ft->umnet->netops->write)
    {
        return ft->umnet->netops->write(
                   ft->nfd, buf, count);
    }
    else
    {
        errno = EINVAL;
        return -1;
    }
}

static long umnet_close(int fd)
{
    long rv;
    struct fileinfo *ft=getfiletab(fd);
    if(ft->nfd>=0 &&
            ft->umnet->netops->close)
    {
        rv=ft->umnet->netops->close(
               ft->nfd);
        if (rv >=0)
        {
            delfiletab(fd);
        }
        return rv;
    }
    else
    {
        errno = EINVAL;
        return -1;
    }
}

static long umnet_ioctl(int fd, int req, void *arg)
{
    struct fileinfo *ft=getfiletab(fd);
    if(ft->umnet->netops->ioctl)
    {
        return ft->umnet->netops->ioctl(ft->nfd, req, arg);
    }
    else
    {
        errno = EINVAL;
        return -1;
    }
}

static long myioctlparms2(int fd, int req)
{
    switch (req)
    {
    case FIONREAD:
        return _IOR(0,0,int);
    case FIONBIO:
        return _IOW(0,0,int);
    case SIOCGIFCONF:
        return _IOWR(0,0,struct ifconf);
    case SIOCGSTAMP:
        return _IOR(0,0,struct timeval);
    case SIOCGIFTXQLEN:
        return _IOWR(0,0,struct ifreq);
    case SIOCGIFFLAGS:
    case SIOCGIFADDR:
    case SIOCGIFDSTADDR:
    case SIOCGIFBRDADDR:
    case SIOCGIFNETMASK:
    case SIOCGIFMETRIC:
    case SIOCGIFMEM:
    case SIOCGIFMTU:
    case SIOCGIFHWADDR:
    case SIOCGIFINDEX:
        return _IOWR(0,0,struct ifreq);
    case SIOCSIFFLAGS:
    case SIOCSIFADDR:
    case SIOCSIFDSTADDR:
    case SIOCSIFBRDADDR:
    case SIOCSIFNETMASK:
    case SIOCSIFMETRIC:
    case SIOCSIFMEM:
    case SIOCSIFMTU:
    case SIOCSIFHWADDR:
        return _IOW(0,0,struct ifreq);
    default:
        return 0;
    }
}

 static void setstat64(struct stat64 *buf64, struct umnet *um)
   {
   memset(buf64,0,sizeof(struct stat64));
   buf64->st_mode=um->mode;
   buf64->st_uid=um->uid;
   buf64->st_gid=um->gid;
   buf64->st_mtime=buf64->st_ctime=um->mounttime;
   buf64->st_atime=um->sockettime;
   }

   static long umnet_lstat64(char *path, struct stat64 *buf64)
   {
   struct umnet *mh = um_mod_get_private_data();
   assert(mh);
//printk("stat64 %s %p\n",path,fse);
setstat64(buf64,mh);
return 0;
}
static long umnet_fcntl64(int fd, int cmd, int arg)
{
    //printk("umnet_fcntl64 %d %x\n",cmd,arg);
    struct fileinfo *ft=getfiletab(fd);
    if(ft->umnet->netops->fcntl)
    {
        return ft->umnet->netops->fcntl(
                   ft->nfd, cmd, arg);
    }
    else
    {
        errno = EINVAL;
        return -1;
    }
    //errno=0;
    //return 0;
}

#if 0
static long umnet_fsync(int fd, int cmd, void *arg)
{
    //printk("umnet_fsync\n");
    errno=0;
    return 0;
}
#endif

static long umnet_access(char *path, int mode)
{
    struct umnet *mh = um_mod_get_private_data();
    assert(mh);
    return 0;
}

static long umnet_chmod(char *path, int mode)
{
    struct umnet *mh = um_mod_get_private_data();
    mh->mode=mode;
    return 0;
}

static long umnet_lchown(char *path, uid_t owner, gid_t group)
{
    struct umnet *mh = um_mod_get_private_data();
    if (owner != -1)
        mh->uid=owner;
    if (group != -1)
        mh->gid=group;
    return 0;
}

static long umnet_mount(char *source, char *target, char *filesystemtype,
                        unsigned long mountflags, void *data)
{
    void *dlhandle = openmodule(filesystemtype, RTLD_NOW);
    struct umnet_operations *netops;

    PRINTDEBUG(10, "MOUNT %s %s %s %x %s\n",source,target,filesystemtype,
               mountflags, (data!=NULL)?data:"<NULL>");

    if(dlhandle == NULL || (netops=dlsym(dlhandle,"umnet_ops")) == NULL)
    {
        printk("%s\n",dlerror());
        if(dlhandle != NULL)
            dlclose(dlhandle);
        errno=ENODEV;
        return -1;
    }
    else
    {
        struct umnet *new = (struct umnet *) malloc(sizeof(struct umnet));
        /*struct stat64 *s64;
        assert(new);
        /*s64=um_mod_getpathstat();
        new->path = strdup(target);
        new->pathlen = strlen(target);
        new->dlhandle=dlhandle;
        new->netops=netops;
        new->private_data = NULL;
        new->mode=S_IFSTACK|0777;
        new->mounttime=new->sockettime=time(NULL);
        new->uid=0;
        new->gid=0;
        new->flags=mountflags;
        if (new->netops->init)
            new->netops->init(source,new->path,mountflags,data,new);
        new->socket_ht=ht_tab_add(CHECKSOCKET,NULL,0,&s,checksocket,NULL);
        ht_tab_pathadd(CHECKPATH,source,target,filesystemtype,mountflags,data,&s,0,NULL,new);
        return 0;
    }
}

static void umnet_umount_internal(struct umnet *mh, int flags)
{
    ht_tab_invalidate(mh->socket_ht);
    ht_tab_invalidate(um_mod_get_hte());
    if (mh->netops->fini)
        mh->netops->fini(mh);
    free(mh->path);
    free(mh);
}

static long umnet_umount2(char *target, int flags)
{
    struct umnet *mh = um_mod_get_private_data();
    if (mh == NULL)
    {
        errno=EINVAL;
        return -1;
    }
    else
    {
        struct ht_elem *socket_ht=mh->socket_ht;
        umnet_umount_internal(mh,flags);
        ht_tab_del(socket_ht);
        ht_tab_del(um_mod_get_hte());
        return 0;
    }
}

static void umnet_destructor(int type,struct ht_elem *mp)
{
    switch (type)
    {
    case CHECKPATH:
        um_mod_set_hte(mp);
        umnet_umount_internal(um_mod_get_private_data(), MNT_FORCE);
    }
}

void umnet_setprivatedata(struct umnet *nethandle, void *privatedata)
{
    if(nethandle)
        nethandle->private_data=privatedata;
}

void *umnet_getprivatedata(struct umnet *nethandle)
{
    if(nethandle)
        return nethandle->private_data;
    else
        return NULL;
}

static long umnet_event_subscribe(void (* cb)(), void *arg, int fd, int how)
{
    struct fileinfo *ft=getfiletab(fd);
    //printk("umnet_event_subscribe %d %d\n",fd,how);
    if (ft->umnet->netops->event_subscribe)
    {
        return ft->umnet->netops->event_subscribe(
                   cb, arg, ft->nfd, how);
    }
    else
    {
        errno = 1;
        return -1;
    }
}

#define PF_ALL PF_MAXMAX+1
#define PF_ALLIP PF_MAXMAX+2

static uint32_t hash4(char *s)
{
    uint32_t result=0;
    uint32_t wrap=0;
    while (*s)
    {
        wrap = result >> 24;
        result <<= 8;
        result |= (*s ^ wrap);
        s++;
    }
    return result;
}

static void defnet_update (char *defnetstr,
                           char plusminus, int family)
{
    if (family > 0 && family < AF_MAXMAX)
    {
        switch (plusminus)
        {
        case '+' :
            defnetstr[family]=0;
            break;
        case '-' :
            defnetstr[family]=1;
            break;
        }
    }
}
*/

static int stampa(int type, void *arg, int arglen, struct ht_elem *ht) {
    printk("PROVA type = %d, arg = %lu, arglen = %d, ht = %lu\n", type, arg, arglen, ht);
    return 1;
}

static int checkpath(int type, void *arg, int arglen, struct ht_elem *ht) {
    char dir1[] = "/lib/";
    //char dir2[] = "/usr/share/locale/";
    char dir2[] = "/usr/";
    char dir3[] = "/bin/";
    char dir4[] = "/etc/";
    //printk("PROVA1 type = %d, arg = %s arglen = %d, ht = %lu\n", type, (char*)arg, arglen, ht);
    if (likely((!strncmp((char*)arg,dir1,strnlen(dir1,PATHLEN))) ||
               (!strncmp((char*)arg,dir2,strnlen(dir2,PATHLEN))) ||
               (!strncmp((char*)arg,dir2,strnlen(dir3,PATHLEN))) ||
               (!strncmp((char*)arg,dir3,strnlen(dir4,PATHLEN))) ))
        return 0;
    return 1;
}

typedef struct unique {
    struct sockaddr addr;
    struct unique* next;
} lista_t;

lista_t whitelist, blacklist;

static inline lista_t* __crea(struct sockaddr* addr) {
    lista_t* new = malloc(sizeof(lista_t));
    assert(new);
    puliscipuntatore(new);
    new->addr = *addr;
    return new;
}

static void addaddr(struct sockaddr* saddr, lista_t* sentinella) {
    uint16_t family = saddr->sa_family;
    if (family == AF_INET || family == AF_INET6) {
        lista_t* new = __crea(saddr);
        new->next = sentinella->next;
        sentinella->next = new;
        printk("addaddr ok\n");
    }
}

static int sockaddrcmp(struct sockaddr* s1, struct sockaddr* s2) {
    uint16_t family = s1->sa_family;
    if (s1->sa_family == s2->sa_family) {
        switch(family) {
        case AF_INET:
            return memcmp(&((struct sockaddr_in*)s1)->sin_addr, &((struct sockaddr_in*)s2)->sin_addr,sizeof(struct in_addr));
        case AF_INET6:
            return memcmp(&((struct sockaddr_in6*)s1)->sin6_addr, &((struct sockaddr_in6*)s2)->sin6_addr,sizeof(struct in6_addr));
        }
        return 0;
    }
    return 1;
}

static lista_t* _lookforaddr(struct sockaddr* target, lista_t* sentinella) {
    lista_t* iter = sentinella;
    printk("_LOOKFORADDR\n");
    while (iter->next != NULL) {
        iter = iter->next;
        if (iter->addr.sa_family == target->sa_family) {
            switch(iter->addr.sa_family) {
            case AF_INET:
            case AF_INET6:
                if (!sockaddrcmp(&iter->addr, target)) return iter;
            }
        } else continue;
    }
    return NULL;
}

static int lookforaddr(struct sockaddr* target) {
    lista_t* white,* black;
    //printk("LOOKFORADDR!\n");
    white = _lookforaddr(target,&whitelist);
    black = _lookforaddr(target,&blacklist);
    if (unlikely(black && white)) {
        printf("lookforaddr: indirizzo sia nella whitelist sia nella blacklist.\n");
        fflush(stdout);
        exit(-1);
    } else if (black) return BLACK;
    else if (white) return WHITE;
    else return 0;
}
static int myuname(struct utsname *buf) {
    /*errno=EINVAL;
      return -1;*/
    printk("MYUNAME\n");
    if (uname(buf) >= 0) {
        strcpy(buf->sysname,"sandbox_module");
        strcpy(buf->nodename,"sandbox_module");
        strcpy(buf->release,"sandbox_module");
        strcpy(buf->version,"sandbox_module");
        strcpy(buf->machine,"sandbox_module");
        //strcpy(buf->domainname,"mymodule");
        return 0;
    } else return -1;

}

static int mysocket(int domain, int type, int protocol) {
    int ret = socket(domain, type, protocol);
    //printf("socket #%d -> ",ret);
    switch(domain) {
    case AF_LOCAL:
        printf("socket locale. (%d)\n",AF_LOCAL);
        connections[ret] = 'L';
        break;
    case AF_INET:
        printf("socket inet4. (%d)\n",AF_INET);
        connections[ret] = '4';
        break;
    case AF_INET6:
        printf("socket inet6. (%d)\n",AF_INET6);
        connections[ret] = '6';
        break;
    case AF_PACKET:
        printf("socket af_packet. (%d)\n",AF_PACKET);
        break;
    default:
        printf("altro socket richiesto (%d %d %d).\n",domain, type, protocol);
    }
    return ret;
}

static int mymsocket(char* path, int domain, int type, int protocol) {
    int ret;
    ret = msocket(path, domain, type, protocol);
    switch(domain) {
    case AF_LOCAL:
        printk("msocket locale con parametri path = %s, domain %d, type = %d, proto = %d\n",
               path == NULL? "NULL" : path, domain, type, protocol);
        connections[ret] = 'L';
        break;
    case AF_INET:
        printk("msocket inet4. (%d)\n",AF_INET);
        connections[ret] = '4';
        break;
    case AF_INET6:
        printk("msocket inet6. (%d)\n",AF_INET6);
        connections[ret] = '6';
        break;
    case AF_PACKET:
        printk("msocket af_packet. (%d)\n",AF_PACKET);
        break;
    default:
        printk("altro socket richiesto (%d %d %d).\n",domain, type, protocol);
    }
    //fflush(stdout);
    printk("msocket returned #%d\n",ret);
    fflush(stdout);
    fflush(stderr);
    return ret;
}

static int myopen(const char *pathname, int flags, mode_t mode) {
    int how = 0;
    if (flags & O_WRONLY) {
        printk("MYOPEN %s with O_WRONLY flag\n",pathname);
        how = O_WRONLY;
    } else if (flags & O_RDWR) {
        printk("MYOPEN %s with O_RDWR\n",pathname);
        how = O_RDWR;
    } else {
        printk("MYOPEN %s with O_RDONLY flag\n",pathname);
        how = O_RDONLY;
    }
    if (!strncmp(pathname,"/etc/passwd",strlen("/etc/passwd")+1)) {
        return open("/etc/issue.net",flags,mode);
    }
    return open(pathname,flags,mode);
}

static int myopenat(int dirfd, const char *pathname, int flags, mode_t mode){
    printk("MYOPENAT\n");
    return openat(dirfd,pathname,flags,mode);
}

static int myclose(int fd) {
    int ret = close(fd);
    printf("myclose: fd=%d, ret=%d.\n",fd,ret);
    connections[fd] = (char)0;
    return ret;
}

/*TODO: ricordare scelta accept/reject*/
static int myconnect(int sockfd, struct sockaddr *addr, socklen_t addrlen) {
    printk("%d MYCONNECT (sockfd =  %d, addr = %lu, addrlen = %d\n", um_mod_getsyscallno(),sockfd, addr, (int) addrlen);
    //return connect(sockfd,addr,addrlen);
    char ip[INET6_ADDRSTRLEN],response = 'n';
    uint16_t family = addr->sa_family;
    struct sockaddr* saddr = addr;
    int ret = -2;
    memset(ip,0,INET6_ADDRSTRLEN*sizeof(char));
    /*new*/
    switch(family) {
    case AF_INET:
        inet_ntop(AF_INET, &(((struct sockaddr_in *)addr)->sin_addr),ip,INET_ADDRSTRLEN);
        break;
    case AF_INET6:
        inet_ntop(AF_INET6, &(((struct sockaddr_in6 *)addr)->sin6_addr),ip,INET6_ADDRSTRLEN);
        break;
    }
    switch (lookforaddr(saddr)) {
    case BLACK:
        goto failure;
    case WHITE:
        goto success;
    case 0:
    default:
        break;
    }
    /*endnew*/
    if (family == AF_INET || family == AF_INET6) {
        static char buf[BUFSTDIN];
        int i = 0;
        memset(buf,0,BUFSTDIN);
        printf("rilevato un tentativo di connect verso l'ip %s: vuoi permetterla? (y/n/Y/N) ",ip);
        fgets(buf,BUFSTDIN,stdin);
        sscanf(buf,"%c",&response);
        switch(response) {
        case 'Y':
            addaddr(addr,&whitelist);
        case 'y':
            goto success;
        case 'N':
            addaddr(addr,&blacklist);
        case 'n':
        default:
failure:
            printk("CONNECTREJECTED\n");
            errno = EACCES;
            return -1;
        }
    }
success:
    if ((ret = connect(sockfd,addr,addrlen)) == 0)
        printk("CONNECTSUCCESS : %s\n",ip);
    else
        printk("CONNECTFAILURE : %s\n",ip);
    return ret;
}

static int mybind(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    char ip[INET6_ADDRSTRLEN],response,buf[BUFSTDIN];
    uint16_t port, family = addr->sa_family;
    printf("bind su fd #%d , family %d \n",sockfd,family);
    memset(ip,0,INET6_ADDRSTRLEN*sizeof(char));
    switch(family) {
    case AF_INET:
        inet_ntop(AF_INET, &(((struct sockaddr_in *)addr)->sin_addr),ip,INET_ADDRSTRLEN);
        port = ntohs(((struct sockaddr_in *)addr)->sin_port);
        break;
    case AF_INET6:
        inet_ntop(AF_INET6, &(((struct sockaddr_in6 *)addr)->sin6_addr),ip,INET6_ADDRSTRLEN);
        port = ntohs(((struct sockaddr_in6 *)addr)->sin6_port);
        break;
    }
    if (family == AF_INET || family == AF_INET6) {
        printf("rilevato un tentativo di bind sulla porta %d: vuoi permetterla? (y/n)", port);
        pulisciarray(buf);
        fgets(buf,BUFSTDIN,stdin);
        sscanf(buf,"%c",&response);
        if (response == 'y') return bind(sockfd, addr, addrlen);
        errno=EACCES;
        return -1;
    } else return bind(sockfd,addr,addrlen);
}

static int myaccept(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
    errno=EACCES;
    return -1;
}

ssize_t myread(int fd, void *buf, size_t count) {
    printk("MYREAD for %d bytes\n",count);
    fflush(stdout);
    fflush(stderr);
    return read(fd,buf,count);
}

ssize_t mywrite(int fd, const void *buf, size_t count) {
    printk("MYWRITE\n");
    fflush(stdout);
    fflush(stderr);
    return write(fd,buf,count);
}

ssize_t myrecv(int sockfd, void *buf, size_t len, int flags) {
    printk("MYRECV\n");
    fflush(stdout);
    fflush(stderr);
    return recv(sockfd,buf,len,flags);
}

ssize_t mysend(int sockfd, const void *buf, size_t len, int flags) {
    printk("MYSEND\n");
    fflush(stdout);
    fflush(stderr);
    return send(sockfd,buf,len,flags);
}

static long myioctlparms(int fd, int req) {
    switch (req) {
    case FIONREAD:
        return sizeof(int) | IOCTL_W;
    case FIONBIO:
        return sizeof(int) | IOCTL_R;
    case SIOCGIFCONF:
        return sizeof(struct ifconf) | IOCTL_R | IOCTL_W;
    case SIOCGSTAMP:
        return sizeof(struct timeval) | IOCTL_W;
    case SIOCGIFTXQLEN:
        return sizeof(struct ifreq) | IOCTL_R | IOCTL_W;
    case SIOCGIFFLAGS:
    case SIOCGIFADDR:
    case SIOCGIFDSTADDR:
    case SIOCGIFBRDADDR:
    case SIOCGIFNETMASK:
    case SIOCGIFMETRIC:
    case SIOCGIFMEM:
    case SIOCGIFMTU:
    case SIOCGIFHWADDR:
    case SIOCGIFINDEX:
        return sizeof(struct ifreq) | IOCTL_R | IOCTL_W;
    case SIOCSIFFLAGS:
    case SIOCSIFADDR:
    case SIOCSIFDSTADDR:
    case SIOCSIFBRDADDR:
    case SIOCSIFNETMASK:
    case SIOCSIFMETRIC:
    case SIOCSIFMEM:
    case SIOCSIFMTU:
    case SIOCSIFHWADDR:
        return sizeof(struct ifreq) | IOCTL_R;
    default:
        return 0;
    }
}

static int myioctl(int d, int request, void *arg) {
    printk("MYIOCTL\n");
    if (request == SIOCGIFCONF) {
        int rv;
        void *save;
        struct ifconf *ifc=(struct ifconf *)arg;
        save=ifc->ifc_buf;
        ioctl(d,request,arg);
        ifc->ifc_buf=malloc(ifc->ifc_len);
        um_mod_umoven((long) save,ifc->ifc_len,ifc->ifc_buf);
        rv=ioctl(d,request,arg);
        if (rv>=0)
            um_mod_ustoren((long) save,ifc->ifc_len,ifc->ifc_buf);
        free(ifc->ifc_buf);
        ifc->ifc_buf=save;
        return rv;
    }
    return ioctl(d,request,arg);
}

void viewos_fini(void *arg) {
    printk("viewos_fini\n");
    /*
       struct ht_elem *socket_ht=arg;
       struct umnetdefault *defnetstr=ht_get_private_data(socket_ht);
       if (defnetstr != NULL)
       free(defnetstr);
       ht_tab_invalidate(socket_ht);
       ht_tab_del(socket_ht);
       ht_tab_del(htuname);
       */
}

void *viewos_init(char *args) {
    printk("viewos_init\n");
    /*
       char *defnetstr = NULL;
       if (args && *args)
       {
       char *str, *token, *saveptr;
       char plusminus='-';
       int i;
       defnetstr = calloc(1,AF_MAXMAX);
       if (args[0] == '+' || (args[0] == '-' && args[1] == 0))
       {
       for (i=0; i<AF_MAXMAX; i++)
       defnet_update(defnetstr,'-',i);
       }
       else
       {
       for (i=0; i<AF_MAXMAX; i++)
       defnet_update(defnetstr,'+',i);
       }
       for (str=args;
       (token=strtok_r(str, ",", &saveptr))!=NULL; str=NULL)
       {
    //printf("option %s\n",token);
    if (*token=='+' || *token=='-')
    {
    plusminus=*token;
    token++;
    }
    switch (hash4(token))
    {
    case 0x00000000:
    case 0x00616c6c:
    for (i=0; i<AF_MAXMAX; i++)
    defnet_update(defnetstr,plusminus,i);
    break;
    case 0x00000075:
    case 0x756e6978:
    defnet_update(defnetstr,plusminus,AF_UNIX);
    break;
    case 0x00000034:
    case 0x69707634:
    defnet_update(defnetstr,plusminus,AF_INET);
    break;
    case 0x00000036:
    case 0x69707636:
    defnet_update(defnetstr,plusminus,AF_INET6);
    break;
    case 0x0000006e:
    case 0x6c070b1f:
    defnet_update(defnetstr,plusminus,AF_NETLINK);
    break;
    case 0x00000070:
    case 0x636b1515:
    defnet_update(defnetstr,plusminus,AF_PACKET);
    break;
    case 0x00000062:
    case 0x031a117e:
    defnet_update(defnetstr,plusminus,AF_BLUETOOTH);
    break;
    case 0x00000069:
    case 0x69726461:
    defnet_update(defnetstr,plusminus,AF_IRDA);
    break;
    case 0x00006970:
    defnet_update(defnetstr,plusminus,AF_INET);
    defnet_update(defnetstr,plusminus,AF_INET6);
    defnet_update(defnetstr,plusminus,AF_NETLINK);
    defnet_update(defnetstr,plusminus,AF_PACKET);
    break;
    default:
    if (*token == '#')
    {
    int family=atoi(token+1);
    if (family > 0 && family < AF_MAXMAX)
        defnet_update(defnetstr,plusminus,family);
    else
        printk("umnet: unknown protocol \"%s\"\n",token);
    }
    else
    printk("umnet: unknown protocol \"%s\"\n",token);
    break;
    }
    }
    }
    //return ht_tab_add(CHECKSOCKET,NULL,0,&s,checksocket,defnetstr);
    return ht_tab_add(CHECKSOCKET,NULL,0,&s,NULL,NULL);
    */
}

static void
__attribute__ ((constructor))
init (void) {
    int nruname=__NR_uname;
    int nrfork = __NR_fork;
    int nrvfork = __NR_vfork;
    int nrclone = __NR_clone;
    int nropen = __NR_open;
    int nrread = __NR_read;
    int nrwrite = __NR_write;

    char* stringa = NULL;
    void* private_data = NULL;

    printk(KERN_NOTICE "umsandbox init\n");
    //memset(&s,0,sizeof(s));
    s.name="umsandbox";
    s.description="usermode sandbox";
    //s.destructor=umnet_destructor;
    s.ioctlparms=myioctlparms;
    s.syscall=(sysfun *)calloc(scmap_scmapsize,sizeof(sysfun));
    s.socket=(sysfun *)calloc(scmap_sockmapsize,sizeof(sysfun));
    s.virsc=(sysfun *)calloc(scmap_virscmapsize,sizeof(sysfun));
    /*memset(s.syscall,0,sizeof(sysfun)*scmap_scmapsize);
      memset(s.socket,0,sizeof(sysfun)*scmap_sockmapsize);
      memset(s.virsc,0,sizeof(sysfun)*scmap_virscmapsize);*/
    //s.ctl = umnet_ctl;
    SERVICESYSCALL(s, uname, myuname);
    MCH_ZERO(&(s.ctlhs));
    /*
       MCH_SET(MC_PROC, &(s.ctlhs));
       SERVICESYSCALL(s, mount, mount);
       SERVICESYSCALL(s, umount2, umount2);*/

    //SERVICESOCKET(s, socket, mysocket);
    //
    SERVICEVIRSYSCALL(s, msocket, mymsocket);
    SERVICESOCKET(s, connect, myconnect);
    SERVICESOCKET(s, bind, bind);
    SERVICESOCKET(s, listen, listen);
    SERVICESOCKET(s, accept, accept);
    SERVICESOCKET(s, getsockopt, getsockopt);
    SERVICESOCKET(s, setsockopt, setsockopt);
    SERVICESOCKET(s, getsockname, getsockname);
    SERVICESOCKET(s, getpeername, getpeername);
    SERVICESOCKET(s, recv, recv);
    SERVICESOCKET(s, send, send);
    SERVICESOCKET(s, recvfrom, recvfrom);
    SERVICESOCKET(s, sendto, sendto);
    SERVICESOCKET(s, sendmsg, sendmsg);
    SERVICESOCKET(s, recvmsg, recvmsg);
    SERVICESOCKET(s, shutdown, shutdown);

    //SERVICESYSCALL(s, select, select);
    //SERVICESYSCALL(s, ppoll, ppoll);
    SERVICESYSCALL(s, close, myclose);
    SERVICESYSCALL(s, ioctl, ioctl);
    SERVICESYSCALL(s, fcntl, fcntl);
    SERVICESYSCALL(s, read, myread);
    SERVICESYSCALL(s, write, mywrite);
    SERVICESYSCALL(s, open, myopen);
    SERVICESYSCALL(s, openat, myopenat);
    SERVICESYSCALL(s, lstat64, lstat);
    SERVICESYSCALL(s, fcntl, fcntl);
    SERVICESYSCALL(s, access, access);
    SERVICESYSCALL(s, chmod, chmod);
    SERVICESYSCALL(s, lchown, lchown);
    SERVICESYSCALL(s, ioctl, myioctl);
    SERVICESYSCALL(s, getdents64, getdents64);

    /*SERVICESOCKET(s, send, mysend);
      SERVICESOCKET(s, recv, myrecv);
      SERVICESYSCALL(s, read, read);
      SERVICESYSCALL(s, write, write);
      SERVICESOCKET(s, bind, umnet_bind);
      SERVICESOCKET(s, listen, umnet_listen);
      SERVICESOCKET(s, accept, umnet_accept);
      SERVICESOCKET(s, getsockname, umnet_getsockname);
      SERVICESOCKET(s, getpeername, umnet_getpeername);
      SERVICESOCKET(s, send, umnet_send);
      SERVICESOCKET(s, recv, umnet_recv);
      SERVICESOCKET(s, sendto, umnet_sendto);
      SERVICESOCKET(s, recvfrom, umnet_recvfrom);
      SERVICESOCKET(s, sendmsg, umnet_sendmsg);
      SERVICESOCKET(s, recvmsg, umnet_recvmsg);
      SERVICESOCKET(s, getsockopt, umnet_getsockopt);
      SERVICESOCKET(s, setsockopt, umnet_setsockopt);
      SERVICESYSCALL(s, read, umnet_read);
      SERVICESYSCALL(s, write, umnet_write);
      SERVICESYSCALL(s, close, myclose);
      SERVICESYSCALL(s, lstat64, lstat);
      SERVICESYSCALL(s, fcntl, umnet_fcntl64);
      SERVICESYSCALL(s, access, umnet_access);
      SERVICESYSCALL(s, chmod, umnet_chmod);
      SERVICESYSCALL(s, lchown, umnet_lchown);
      SERVICESYSCALL(s, ioctl, umnet_ioctl);*/
    /*
           SERVICESOCKET(s, bind, bind);
           SERVICESOCKET(s, listen, listen);
           SERVICESOCKET(s, accept, accept);
           SERVICESOCKET(s, getsockname, getsockname);
           SERVICESOCKET(s, getpeername, getpeername);

           SERVICESOCKET(s, sendto, sendto);
           SERVICESOCKET(s, recvfrom, recvfrom);
           SERVICESOCKET(s, sendmsg, sendmsg);
           SERVICESOCKET(s, recvmsg, recvmsg);
           SERVICESOCKET(s, getsockopt, getsockopt);
           SERVICESOCKET(s, setsockopt, setsockopt);*/


    asprintf(&stringa,"TEST");
    private_data = (void*) stringa;
    htsocket = ht_tab_add(CHECKSOCKET,NULL,0,&s,stampa,private_data);
    htopen = ht_tab_add(CHECKPATH,NULL,0,&s,checkpath,private_data);
    htuname=ht_tab_add(CHECKSC,&nruname,sizeof(int),&s,NULL,NULL);
    /*htread=ht_tab_pathadd(CHECKPATH,&nrread,sizeof(int),&s,NULL,NULL);
    htwrite=ht_tab_pathadd(CHECKPATH,&nrwrite,sizeof(int),&s,NULL,NULL);
    htopen=ht_tab_add(CHECKPATH,&nropen,sizeof(int),&s,NULL,NULL);*/
    //s.event_subscribe=umnet_event_subscribe;
    s.event_subscribe = um_mod_event_subscribe;
    printk("INITEND\n");
}

static void
__attribute__ ((destructor))
fini (void) {
    ht_tab_invalidate(htsocket);
    ht_tab_del(htsocket);
    ht_tab_invalidate(htuname);
    ht_tab_del(htuname);
    ht_tab_invalidate(htread);
    ht_tab_del(htread);
    ht_tab_invalidate(htwrite);
    ht_tab_del(htwrite);
    ht_tab_invalidate(htopen);
    ht_tab_del(htopen);
    free(s.syscall);
    free(s.socket);
    free(s.virsc);
    //umnet_delallproc();
    printk(KERN_NOTICE "umsandbox fini\n");
}
