#include "pch.h"
#include "pch.h"
#include "Art.h"
#include "Messageprint.h"
#include <dlfcn.h>
#include "Hook.h"
#include <fcntl.h>
#include <cstdlib>

//��ǰ�����ϵͳdex2oat �Զ���oat ��ʱû������
static int (*artoldopen)(const char* file, int oflag, ...);

static int (*artoldfstat)(int, struct stat*);
static ssize_t (*artoldread)(int fd, void* des, size_t request);
static ssize_t (*artoldwrite)(int, const void*, size_t);
static void* (*artoldmmap)(void*, size_t, int, int, int, off_t);
static int (*artoldmprotect)(const void*, size_t, int);
static int (*artoldmunmap)(void*, size_t);

//����dex�ļ�·��
static char* dexFilePath;
//����oat�ļ�·��
static char* oatFilePath;
//�������dex key
static char* rc4Key;
//dex �ļ�fd;
static int dexfd = 0;
//oat �ļ�fd ��write�������õ�
static int oatfd = 0;

int artmyfstat(int fd, struct stat* st)
{
	//��������

	return artoldfstat(fd, st);
}

ssize_t artmywrite(int fd, const void* dec, size_t request)
{
	//�������� ��ʼдoat�ļ�
	/*
	 *
	 */
	if (oatfd==-1||fd!=oatfd)
	{
		return  artoldwrite(fd, dec, request);
	}
	ssize_t res = artoldwrite(fd, dec, request);
	Messageprint::printinfo("loaddex", "write oatfile fd:%d  request:%d writeed:%d", fd, request, res);
	return res;
}

int artmymprotect(const void* des, size_t size, int s)
{
	return artoldmprotect(des, size, s);
}

int artmymunmap(void* des, size_t size)
{
	//set mummap fail
	return 0;
}

void* artmymmap(void* start, size_t len, int prot, int flags, int fd, off_t offset)
{
	//�����ԭ����dex�ļ�����mmap ���ڴ�
	//�ж��Ƿ� ûmmap dex�ļ� 
	/* 1. fdΪnull
	 * 2. dexfd=-1
	 * 3. fd������dexfd
	 */
	if (fd== 0xFFFFFFFF||dexfd==-1||fd!=dexfd)
	{
		return  artoldmmap(start, len, prot, flags, fd, offset);
	}
	/*
	 * �����������صļ��� ��ʱ���� ����fd���ص�
	 */
	void* result = artoldmmap(start, len, prot, flags, fd, offset);
	Messageprint::printinfo("loaddex", "mmap dex start:0x%08x port:%d len:0x%08x fd:0x%X offset:0x%x", start, prot, len, fd, offset);
	return result;
}


ssize_t artmyread(int fd, void* dest, size_t request)
{
	//dex2oat���Ż������� ���ȡһ��"dex\n"�ļ�ͷ ���˵�������ȡ
	if (dexfd==-1||request!=4||fd!=dexfd)
	{
		return artoldread(fd, dest, request);
	}
	memcpy(dest, "dex\n", 4u);
	Messageprint::printinfo("loaddex", "read dex magic success");
	return 4;
}

static int artmyopen(const char* pathname, int flags, ...)
{
	//��������
	mode_t mode = 0;
	if ((flags & O_CREAT) != 0)
	{
		va_list args;
		va_start(args, flags);
		mode = static_cast<mode_t>(va_arg(args, int));
		va_end(args);
	}
	//��dexfd �����ж�
	if (dexfd!=0&&oatfd!=0)
	{
		return  artoldopen(pathname, flags, mode);;
	}
	if (strcmp(dexFilePath, pathname) == 0)
	{
		dexfd = artoldopen(pathname, flags, mode);
		Messageprint::printinfo("loaddex", "open dex:%s fd:%d", pathname, dexfd);
		return dexfd;
	}
	else if (strcmp(oatFilePath, pathname) == 0)
	{
		oatfd = artoldopen(pathname, flags, mode);
		Messageprint::printinfo("loaddex", "open oat:%s fd:%d", pathname, oatfd);
		return oatfd;
	}
	else
	{
		return  artoldopen(pathname, flags, mode);;
	}
}


void getEnvText()
{
	dexFilePath = getenv("DEX_PATH");
	rc4Key = getenv("key");
	oatFilePath = getenv("OAT_PATH");
	Messageprint::printinfo("loaddex", "dexpath:%s", dexFilePath);
}

void art::InitLogging(char* argv[])
{
	if (stophook)
	{
		oldInitLogging(argv);
	}
	getEnvText();
	void* arthandle = dlopen("libart.so", 0);
	oldInitLogging = (void(*)(char**))dlsym(arthandle, "_ZN3art11InitLoggingEPPc");
	Hook::hookMethod(arthandle, "open", (void*)artmyopen, (void**)(&artoldopen));
	Hook::hookMethod(arthandle, "read", (void*)artmyread, (void**)(&artoldread));
	//Hook::hookMethod(arthandle, "fstat", (void*)artmyfstat, (void**)(&artoldfstat));
	Hook::hookMethod(arthandle, "mmap", (void*)artmymmap, (void**)(&artoldmmap));
	//Hook::hookMethod(arthandle, "mprotect", (void*)artmymprotect, (void**)(&artoldmprotect));
	Hook::hookMethod(arthandle, "write", (void*)artmywrite, (void**)(&artoldwrite));
#if defined(__arm__)
	Hook::hookAllRegistered();
#endif
	//dlclose(arthandle);
	return oldInitLogging(argv);
}
