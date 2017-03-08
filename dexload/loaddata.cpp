#include "pch.h"
#include "loaddata.h"
#include "util.h"
#include "Messageprint.h"
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <cstdio>
#include <dirent.h>;
#include <sys/stat.h>
#include <dlfcn.h>
#include "Hook.h"
#include <string>
#include <cstdlib>
#include "dexload.h"
#include <asm-generic/fcntl.h>
char* PackageFilePath;
char* PackageNames;

char* oatfilePath;

loaddata::loaddata()
{
}


loaddata::~loaddata()
{
}

void loaddata::run(JNIEnv* env, jobject obj, jobject ctx)
{
	jclass ActivityThread = env->FindClass("android/app/ActivityThread");
	jmethodID currentActivityThread = env->GetStaticMethodID(ActivityThread, "currentActivityThread", "()Landroid/app/ActivityThread;");
}


void loaddata::attachContextBaseContext(JNIEnv* env, jobject obj, jobject ctx)
{
	//��ȡdata/data/packageName/File�ļ���
	// //6.0Ϊ/data/user/0/packagename/files/Ŀ¼
	jclass ApplicationClass = env->GetObjectClass(ctx);
	jmethodID getFilesDir = env->GetMethodID(ApplicationClass, "getFilesDir", "()Ljava/io/File;");
	jobject File_obj = env->CallObjectMethod(ctx, getFilesDir);
	jclass FileClass = env->GetObjectClass(File_obj);
	jmethodID getAbsolutePath = env->GetMethodID(FileClass, "getAbsolutePath", "()Ljava/lang/String;");
	jstring data_file_dir = static_cast<jstring>(env->CallObjectMethod(File_obj, getAbsolutePath));
	const char* cdata_file_dir = Util::jstringTostring(env, data_file_dir);
	//�ͷ�
	env->DeleteLocalRef(data_file_dir);
	env->DeleteLocalRef(File_obj);
	env->DeleteLocalRef(FileClass);

	//NativeLibraryDir ��ȡlib�����ļ���
	jmethodID getApplicationInfo = env->GetMethodID(ApplicationClass, "getApplicationInfo", "()Landroid/content/pm/ApplicationInfo;");
	jobject ApplicationInfo_obj = env->CallObjectMethod(ctx, getApplicationInfo);
	jclass ApplicationInfoClass = env->GetObjectClass(ApplicationInfo_obj);
	jfieldID nativeLibraryDir_fied = env->GetFieldID(ApplicationInfoClass, "nativeLibraryDir", "Ljava/lang/String;");
	jstring nativeLibraryDir = static_cast<jstring>(env->GetObjectField(ApplicationInfo_obj, nativeLibraryDir_fied));
	const char* cnativeLibraryDir = Util::jstringTostring(env, nativeLibraryDir);
	//�ͷ�
	env->DeleteLocalRef(nativeLibraryDir);
	env->DeleteLocalRef(ApplicationInfoClass);
	env->DeleteLocalRef(ApplicationInfo_obj);

	//��ȡapk ����·�� ����getPackageResourcePath
	jmethodID getPackageResourcePath = env->GetMethodID(ApplicationClass, "getPackageResourcePath", "()Ljava/lang/String;");
	jstring mPackageFilePath = static_cast<jstring>(env->CallObjectMethod(ctx, getPackageResourcePath));
	const char* cmPackageFilePath = Util::jstringTostring(env, mPackageFilePath);
	PackageFilePath = const_cast<char*>(cmPackageFilePath);
	env->DeleteLocalRef(mPackageFilePath);

	//��ȡ����
	jmethodID getPackageName = env->GetMethodID(ApplicationClass, "getPackageName", "()Ljava/lang/String;");
	jstring PackageName = static_cast<jstring>(env->CallObjectMethod(ctx, getPackageName));
	const char* packagename = Util::jstringTostring(env, PackageName);
	PackageNames = (char*)packagename;
	env->DeleteLocalRef(PackageName);

	//�ص� �õ�ClassLoader
	jmethodID getClassLoader = env->GetMethodID(ApplicationClass, "getClassLoader", "()Ljava/lang/ClassLoader;");
	jobject classLoader = env->CallObjectMethod(ctx, getClassLoader);

	char codePath[256] = {0};
	sprintf(codePath, "%s/%s", cdata_file_dir, "code");
	//����dex�ļ�����ȡ�������߻�ȡ�Ѿ�����dex�ļ�������
	int dexnums = ExtractFile(env, ctx, codePath);
	//free(&codePath);
	if (dexnums <= 0)
	{
		return;
	}
	//����dex 

	jclass DexFile = env->FindClass("dalvik/system/DexFile");
	jfieldID mCookie;
	std::string cookietype = Util::getmCookieType(env);
	//���mCookie ֵ�����Ͳ�ͬ���԰汾�������� 
	mCookie = env->GetFieldID(DexFile, "mCookie", cookietype.c_str());

	MethodSign method_sign = Util::getMehodSign(env, "dalvik.system.DexFile", "loadDex");
	//������ �õ�openDexFileNative ���� ���Ƿ���4.0�Ļ���������������openDexFileNative  so ��ΪopenDexFile
	jmethodID openDexFileNative = env->GetStaticMethodID(DexFile, "loadDex", method_sign.sign.c_str());
	//Messageprint::printinfo(__FUNCTION__, "over here:%d", __LINE__);
	loaddex(env, openDexFileNative, cdata_file_dir, method_sign.argSize, dexnums, cookietype.c_str(), classLoader);
}

//�������ܵ�dex�ļ��������ж��dex
/*
*env
*ctx Context
*path /data/data/packageName/files/code�ļ���
*/
int loaddata::ExtractFile(JNIEnv* env, jobject ctx, const char* path)
{
	if (access(path, F_OK) == -1)
	{
		mkdir(path, 505);
		chmod(path, 505);
		//�õ�AAssetManager
		AAssetManager* mgr;
		jclass ApplicationClass = env->GetObjectClass(ctx);
		jmethodID getAssets = env->GetMethodID(ApplicationClass, "getAssets", "()Landroid/content/res/AssetManager;");
		jobject Assets_obj = env->CallObjectMethod(ctx, getAssets);
		mgr = AAssetManager_fromJava(env, Assets_obj);
		if (mgr == nullptr)
		{
			Messageprint::printerror("ExtractFile", "AAssetManager_fromJava fail");
			return 0;
		}
		//�����assetsĿ¼
		AAssetDir* dirs = AAssetManager_openDir(mgr, "");
		//AAsset* asset = AAssetManager_open(mgr, "dump.dex", AASSET_MODE_STREAMING);
		const char* FileName;
		int i = 0;
		while ((FileName = AAssetDir_getNextFileName(dirs)) != nullptr)
		{
			if (strstr(FileName, "encrypt") != nullptr && strstr(FileName, "dex") != nullptr)
			{
				AAsset* asset = AAssetManager_open(mgr, FileName, AASSET_MODE_STREAMING);
				FILE* file;
				void* buffer;
				int numBytesRead;
				if (asset != nullptr)
				{
					char filePath[256] = {0};
					sprintf(filePath, "%s/%s", path, FileName);
					file = fopen(filePath, "wb");
					//       int bufferSize = AAsset_getLength(asset); 
					//       LOGI("buffersize is %d",bufferSize);
					buffer = malloc(1024);
					while (true)
					{
						numBytesRead = AAsset_read(asset, buffer, 1024);
						if (numBytesRead <= 0)
							break;
						fwrite(buffer, numBytesRead, 1, file);
					}
					free(buffer);
					fclose(file);
					AAsset_close(asset);
					i = i + 1;
					chmod(filePath, 493);
				}
				else
				{
					Messageprint::printerror("ExtractFile", "AAsset is null :%s", FileName);
				}
			}
		}
		return i;
	}
	else//��ȡdex��Ŀ
	{
		DIR* dir = opendir(path);
		struct dirent* direntp;
		int i = 0;
		if (dir != nullptr)
		{
			for (;;)
			{
				direntp = readdir(dir);
				if (direntp == nullptr) break;
				//printf("%s\n", direntp->d_name);
				if (strstr(direntp->d_name, "encrypt") != nullptr && strstr(direntp->d_name, "dex") != nullptr)
				{
					i = i + 1;
				}
			}
			closedir(dir);
			return i;
		}
		Messageprint::printinfo("ExtractFile", "dir existed");
	}

	return 0;
}


//------------------------------hook----------------------------------start
static bool stophook = false;
static int testoatfd = 0;
ssize_t (*oldread)(int fd, void* dest, size_t request);
ssize_t myread(int fd, void* dest, size_t request)
{
	if (fd==testoatfd&&request==4)
	{
		memcpy(dest, "dex\n", 4u);
		return  4;
	}
	return  oldread(fd, dest, request);
}

static int (*oldopen)(const char* pathname, int flags, ...);
static int myopen(const char* pathname, int flags, ...)
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

	if (strcmp(pathname,"/data/user/0/com.xiaobai.loaddextest/files/optdir/encrypt0.so")==0)
	{
		testoatfd= oldopen(pathname, flags, mode);
		Messageprint::printinfo("loaddex", "open:%s fd:%d", pathname, testoatfd);
		return testoatfd;
	}
	
	

	return  oldopen(pathname, flags, mode);
}

static void hookSomeTest()
{
	/*void* arthandle = dlopen("libart.so", 0);
	Hook::hookMethod(arthandle, "open", (void*)myopen, (void**)&oldopen);
	Hook::hookAllRegistered();*/
}

//------------------------------hook------------------------------------end
//�˴���ʼ����dex �����Ƚ϶�
/*2017��3��6��11:10:08��������
 *openDexFileNative ����ָ��
 *opt �Ż����Ŀ¼
 *argSize openDexFileNative������������
 *dexnums �����ٸ�dex
 *cooketype ���������Ǹ����� 
 *classLoader Application.getClassLoader();
 */
void loaddata::loaddex(JNIEnv* env, jmethodID openDexFileNative, const char* data_filePath, int argSize, int dexnums, const char* cooketype, jobject/*for android 7.0*/ classLoader)
{
	//2017��3��6��11:10:22��һ���Ż�

	hookSomeTest();
	// for android 7.0  argsize=5
	jclass DexFile = env->FindClass("dalvik/system/DexFile");
	char* coptdir = new char[256];
	memset(coptdir, 0, 256);
	//data/data/packageName/files/opt/�ļ���
	sprintf(coptdir, "%s/%s", data_filePath, "optdir");
	if (access(coptdir, F_OK) == -1)
	{
		mkdir(coptdir, 505);
		chmod(coptdir, 505);
	}
	//�ͷ��ڴ�
	//delete[] coptdir;
	//���ݲ������м��� ���7.0ϵͳ û����û����
	//start hook open mmap read witre���� �����Ѿ�����hook ��
	hookSomeTest();
	stophook = false;
	//���7.0 
	if (argSize == 5)
	{
		for (int i = 0; i < dexnums; ++i)
		{
			//dex�ļ�·��
			char* codePath = new char[256];
			//�Ż����·��
			char* copt_string = new char[256];
			memset(copt_string, 0, 256);
			memset(codePath, 0, 256);
			sprintf(codePath, "%s/%s/%s%d.%s", data_filePath, "code", "encrypt", (i), "dex");
			sprintf(copt_string, "%s/%s%d.%s", coptdir, "encrypt", (i), "so");
			jstring oufile = env->NewStringUTF(copt_string);
			jstring infile = env->NewStringUTF(codePath);
			//oat �Ż�
			if (makedex2oat(codePath, copt_string))
			{
				//������Ѷ���� ֱ�Ӽ���oat�ļ��������� null
				//loadDex(String sourcePathName, String outputPathName,int flags, ClassLoader loader, DexPathList.Element[] elements) ����DexFile ���� ����Ӧ�ÿ�һ��
				jobject dexfileobj = env->CallStaticObjectMethod(DexFile, openDexFileNative, infile, oufile, nullptr, classLoader, nullptr);
				makeDexElements(env, classLoader, dexfileobj, 0, 0, nullptr);
			}
			else
			{
				Messageprint::printerror("makedex2oat", "make fail");
			}


			//�ͷ��Ż�
			env->DeleteLocalRef(oufile);

			delete[] codePath;
			delete[] copt_string;
		}
	}
	//7.0 ���� ����Ϊ3��
	else if (argSize == 3)
	{ //�����4.4���µ�ϵͳ ��ʵ�ǿ��Բ���byte[]��ʽ���� ����Ϊ�˷��� ֱ�Ӳ��á���ء���ʽ����
		if (strcmp(cooketype, "I") == 0)
		{
			for (int i = 0; i < dexnums; ++i)
			{
				char* codePath = new char[256];
				char* copt_string = new char[256];
				memset(copt_string, 0, 256);
				memset(codePath, 0, 256);
				// dex �ļ�·�� data/data/packageName/files/code/encrypt.x.dex;
				sprintf(codePath, "%s/%s/%s%d.%s", data_filePath, "code", "encrypt", (i), "dex");
				sprintf(copt_string, "%s/%s%d.%s", coptdir, "encrypt", (i), "dex");
				jstring oufile = env->NewStringUTF(copt_string);
				jstring infile = env->NewStringUTF(codePath);
				//����dex ���õ�cookieֵ
				jint cookies = env->CallStaticIntMethod(DexFile, openDexFileNative, infile, oufile, 0);
				jobject dexfile = Util::newFile(env, codePath);
				makeDexElements(env, classLoader, nullptr, cookies, 0, nullptr);
				//�ͷ��Ż�
				env->DeleteLocalRef(oufile);
				env->DeleteLocalRef(infile);
				env->DeleteLocalRef(dexfile);
				delete[] codePath;
				delete[] copt_string;
			}
		}
		else if (strcmp(cooketype, "J") == 0)
		{
			for (int i = 0; i < dexnums; ++i)
			{
				char* codePath = new char[256];
				char* copt_string = new char[256];
				memset(copt_string, 0, 256);
				memset(codePath, 0, 256);
				// dex file path  data/data/packageName/files/code/encrypt.x.dex;
				sprintf(codePath, "%s/%s/%s%d.%s", data_filePath, "code", "encrypt", (i), "dex");
				sprintf(copt_string, "%s/%s%d.%s", coptdir, "encrypt", (i), "dex");
				jstring oufile = env->NewStringUTF(copt_string);
				jstring infile = env->NewStringUTF(codePath);

				//start load dexfile and get retrun value
				jlong cookies = env->CallStaticLongMethod(DexFile, openDexFileNative, infile, oufile, 0);
				jobject dexfile = Util::newFile(env, codePath);
				makeDexElements(env, classLoader, dexfile, 0, cookies, nullptr);
				//release
				env->DeleteLocalRef(oufile);
				env->DeleteLocalRef(infile);
				env->DeleteLocalRef(dexfile);
				delete[] codePath;
				delete[] copt_string;
			}
		}
		else if (strcmp(cooketype, "Ljava/lang/Object;") == 0)
		{
			for (int i = 0; i < dexnums; ++i)
			{
				char* codePath = new char[256];
				char* copt_string = new char[256];
				memset(copt_string, 0, 256);
				memset(codePath, 0, 256);
				// dex file path data/data/packageName/files/code/encrypt.x.dex;
				sprintf(codePath, "%s/%s/%s%d.%s", data_filePath, "code", "encrypt", (i), "dex");
				sprintf(copt_string, "%s/%s%d.%s", coptdir, "encrypt", (i), "so");
				jstring oufile = env->NewStringUTF(copt_string);
				jstring infile=env->NewStringUTF(codePath);
				if (isArt)
				{
					//dex2oat
					if (makedex2oat(codePath, copt_string))
					{
						jobject dexFile = env->CallStaticObjectMethod(DexFile, openDexFileNative, infile, oufile, 0);
						makeDexElements(env, classLoader, dexFile, 0, 0, nullptr);
					}
					else
					{
						Messageprint::printerror("makedex2oat", "make fail");
					}
				}
				else
				{
					jstring infile = env->NewStringUTF(codePath);
					//start load dexfile and get retrun value
					jobject cookies = env->CallStaticObjectMethod(DexFile, openDexFileNative, infile, oufile, 0);
					jobject dexfile = Util::newFile(env, codePath);
					makeDexElements(env, classLoader, dexfile, 0, 0, cookies);
					//release
					env->DeleteLocalRef(oufile);
					env->DeleteLocalRef(infile);
					env->DeleteLocalRef(dexfile);
				}
				delete[] codePath;
				delete[] copt_string;
			}
		}
	}
	stophook = true;
	// start unhook open mmap 
}

/*
 *
 *
 */
/*
 *classLoader
 *intTypeCookie 
 *longTypeCookie 
 *objectTypeCookie
 *
 */
void loaddata::makeDexElements(JNIEnv* env, jobject classLoader, jobject dexFileobj, jint intTypeCookie, jlong longTypeCookie, jobject objectTypeCookie)
{
	
	//Application.getClassLoader().getClass().getName()  = dalvik.system.PathClassLoader
	/*
	*PathClassLoader superClass BaseDexClassLoader
	*BaseDexClassLoader superClass  ClassLoader
	*/
	
	jclass PathClassLoader = env->GetObjectClass(classLoader);

	jclass BaseDexClassLoader = env->GetSuperclass(PathClassLoader);
	//release
	env->DeleteLocalRef(PathClassLoader);
	//get pathList fiedid
	jfieldID pathListid = env->GetFieldID(BaseDexClassLoader, "pathList", "Ldalvik/system/DexPathList;");
	jobject pathList = env->GetObjectField(classLoader, pathListid);

	//get DexPathList Class 
	jclass DexPathListClass = env->GetObjectClass(pathList);
	//get dexElements fiedid
	jfieldID dexElementsid = env->GetFieldID(DexPathListClass, "dexElements", "[Ldalvik/system/DexPathList$Element;");
	//get dexElement array value
	jobjectArray dexElement = static_cast<jobjectArray>(env->GetObjectField(pathList, dexElementsid));

	//get DexPathList$Element Class construction method and get a new DexPathList$Element object 
	jint len = env->GetArrayLength(dexElement);
	jclass ElementClass = env->FindClass("dalvik/system/DexPathList$Element");
	jmethodID Elementinit = env->GetMethodID(ElementClass, "<init>", "(Ljava/io/File;ZLjava/io/File;Ldalvik/system/DexFile;)V");
	jboolean isDirectory = JNI_FALSE;
	jobject element_obj = env->NewObject(ElementClass, Elementinit, nullptr, isDirectory, nullptr, dexFileobj);

	//Get dexElement all values and add  add each value to the new array
	jobjectArray new_dexElement = env->NewObjectArray(len + 1, ElementClass, nullptr);
	for (int i = 0; i < len; ++i)
	{
		env->SetObjectArrayElement(new_dexElement, i, env->GetObjectArrayElement(dexElement, i));
	}
	//then set dexElement Fied 
	env->SetObjectArrayElement(new_dexElement, len, element_obj);
	env->SetObjectField(pathList, dexElementsid, new_dexElement);
}

bool loaddata::makedex2oat(const char* DEX_PATH, const char* OTA_PATH)
{
	char* test = new char[256];
	memset(test, 0, 256);
	memcpy(test, OTA_PATH, strlen(OTA_PATH));
	oatfilePath =test;
	//if oat file exist retrun true
	if (access(OTA_PATH, F_OK) == -1)
	{
		std::string cmd;
		//DEX_PATH="/data/data/com.xiaobai.loaddextest/files/code/encrypt0.dex" 
		cmd.append("DEX_PATH=\"");
		cmd.append(DEX_PATH);
		cmd.append("\" ");

		//OTA_PATH="/data/local/tmp/test.so"   
		cmd.append("OAT_PATH=\"");
		cmd.append(OTA_PATH);
		cmd.append("\" ");

		//LD_PRELOAD="/data/app/com.catchingnow.icebox-1/lib/arm/libdexload.so"
		cmd.append("LD_PRELOAD=\"");
		char* paths = new char[256];
		memset(paths, 0, 256);
		sprintf(paths, "/data/data/%s/lib/libdexload.so", PackageNames);
		cmd.append(paths);
		cmd.append("\" ");

		cmd.append("/system/bin/dex2oat ");
#if defined(__i386__)
	cmd.append("--instruction-set=x86 ");
#else
		cmd.append("--instruction-set=arm ");
#endif
		//--boot-image=/system/framework/boot.art 
		cmd.append("--boot-image=/system/framework/boot.art ");
		//--dex-file=/data/data/com.xiaobai.loaddextest/files/code/encrypt0.dex 
		cmd.append("--dex-file=");
		cmd.append(DEX_PATH);
		cmd.append(" ");

		//--oat-file=/data/local/tmp/test.so 
		cmd.append("--oat-file=");
		cmd.append(OTA_PATH);
		cmd.append(" ");

		cmd.append("--compiler-filter=interpret-only");

		Messageprint::printinfo("dex2oat", "cmd:%s", cmd.c_str());

		int optres = system(cmd.c_str());
		Messageprint::printinfo("dex2oat", "optres:%d", optres);
		if (access(OTA_PATH, F_OK) == -1)
		{
			Messageprint::printinfo("dex2oat", "opt fail");
			return false;
		}
		else
		{
			Messageprint::printinfo("dex2oat", "opt success");
			return true;
		}
	}
	else
	{
		return true;
	}
}
