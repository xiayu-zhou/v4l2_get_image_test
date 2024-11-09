//
// Created by Kiana on 2024/5/21.
//

/*
 * 1.打开设备
 * 2.配置（获取支持格式）
 * 3.申请内核缓冲区队列
 * 4.把内核缓冲区队列映射到用户控件
 * 5.开始采集
 * 6.采集数据
 * 7.停止采集
 * 8.释放映射
 * 9.关闭设备
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/poll.h>

#define PATH_NAME "/dev/video0"


int main(void) {

    ///--------------1.打开设备
    int fd;
    int ret;
    fd = open(PATH_NAME, O_RDWR);
    if (fd < 0) {
        printf("ERR(%s):failed to open %s\n", __func__, PATH_NAME);
        return -1;
    }
    printf("fd = %d\n", fd);

    ///--------------2.配置（获取支持格式）
    //获取摄像头支持的格式 VIDIOC_ENUM_FMT
    struct v4l2_fmtdesc fmtdesc;
    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmtdesc.index = 0;
    while (!ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc))
    {
        printf("fmt:%s\n", fmtdesc.description);
        fmtdesc.index++;
    }
    //设置图片大小采集格式
    struct v4l2_format v4l2_fmt;
    int width = 1280;
    int height = 720;
    memset(&v4l2_fmt, 0, sizeof(struct v4l2_format));
    v4l2_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    v4l2_fmt.fmt.pix.width = width; //宽度
    v4l2_fmt.fmt.pix.height = height; //高度
    v4l2_fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG; //像素格式
    //v4l2_fmt.fmt.pix.field = V4L2_FIELD_ANY;
    if (ioctl(fd, VIDIOC_S_FMT, &v4l2_fmt) < 0)
    {
        printf("ERR(%s):VIDIOC_S_FMT failed\n", __func__);
        return -1;
    }
    //获取采集格式
    memset(&v4l2_fmt, 0, sizeof(struct v4l2_format));
    v4l2_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_G_FMT, &v4l2_fmt) < 0)
    {
        printf("Failed to get format\n");
        return -1;
    }

    printf("width = %d\n",v4l2_fmt.fmt.pix.width);
    printf("height = %d\n",v4l2_fmt.fmt.pix.height);

    ///--------------3.申请内核缓冲区队列
    struct v4l2_requestbuffers req;
    req.count = 4; //缓存数量
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;//映射方式
    if (ioctl(fd, VIDIOC_REQBUFS, &req) < 0)
    {
        printf("The application queue failed\n");
        return -1;
    }

    ///--------------4.映射到用户空间
    unsigned char *mptr[4];
    unsigned int  size[4];
    struct v4l2_buffer v4l2_buf;
    v4l2_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    //v4l2_buffer.memory = V4L2_MEMORY_MMAP;
    for (int i = 0; i < 4; ++i) {
/*        memset(&v4l2_buffer, 0, sizeof(struct v4l2_buffer));*/
        v4l2_buf.index = i; //想要放入队列的缓存
        ret = ioctl(fd, VIDIOC_QUERYBUF, &v4l2_buf);
        if(ret < 0)
        {
            printf("Unable to queue buffer.\n");
            return -1;
        }
        mptr[i] = (unsigned char *)mmap(NULL,v4l2_buf.length,PROT_READ|PROT_WRITE,MAP_SHARED,fd,v4l2_buf.m.offset);
        size[i]=v4l2_buf.length;
        ret = ioctl(fd, VIDIOC_QBUF, &v4l2_buf);
        if(ret < 0)
        {
            printf("Unable to queue buffer.\n");
            return -1;
        }
    }

    ///--------------5.开始采集
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ret = ioctl(fd,VIDIOC_STREAMON,&type);
    if(ret < 0)
    {
        printf("Failed to start acquisition\n");
    }

    ///--------------6.采集数据
    //获取一帧数据
    struct v4l2_buffer r_buf;
    r_buf.type = type;
    ret = ioctl(fd,VIDIOC_DQBUF,&r_buf);
    if(ret < 0)
    {
        printf("Data extraction failed\n");
    }
    FILE  *file = fopen("my.jpg","w+");
    //mptr[r_buf.index]
    fwrite(mptr[r_buf.index],r_buf.length,1,file);
    fclose(file);
    //使用完毕
    ret = ioctl(fd,VIDIOC_QBUF,&r_buf);
    if(ret < 0)
    {
        printf("Failed to put back in queue\n");
    }

    ///--------------7.停止采集数据
    ret = ioctl(fd,VIDIOC_STREAMOFF,&type);

    ///--------------8.释放映射
    for(int i=0; i<4; i++)
        munmap(mptr[i], size[i]);

    ///--------------关闭设备
    //getchar();
    close(fd);
    return 0;
}
