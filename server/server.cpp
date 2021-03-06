/* File Name: server.cpp */
#include "server.h"

#ifdef HDFS_FLAG
#include "hdfs.h"
#endif

#define		SERVER_CONF			"server.conf"

#ifdef HDFS_FLAG
char *hdfs_path = NULL, *hdfs_addr = NULL;
pthread_mutex_t g_mutex_qwrite_hdfs;
#endif
 
char *log_path = NULL, *save_path = NULL;
pthread_mutex_t g_mutex_setflag;
pthread_t *id = NULL;
FILE *log_fd = NULL;
int hdfs_port = 0;
hdfsFS fs = NULL;

queue <string> qwrite_hdfs;

int get_hdfs_path(int now_min)
{
	int hdfs_path_p = HDFS_PATH_PER_0;

	if ((now_min >= HDFS_PATH_PER_0) && (now_min < HDFS_PATH_PER_20))
		hdfs_path_p = HDFS_PATH_PER_0;
	else if ((now_min >= HDFS_PATH_PER_20) && (now_min < HDFS_PATH_PER_40))
		hdfs_path_p = HDFS_PATH_PER_20;
	else
		hdfs_path_p = HDFS_PATH_PER_40;

	return hdfs_path_p;
}

int write_HDFS(string filename)
{
	char writepath[MIDLENGTH] = { 0 }, buffer[MAXLENGTH] = { 0 }, temp1[20] = { 0 };
	hdfsFile writeFile;
	FILE *fp = NULL;

	sprintf(writepath, "%s/%s", hdfs_path, basename((char *)filename.c_str()));
	writeFile = hdfsOpenFile(fs, writepath, O_WRONLY | O_CREAT, MAXLENGTH, 0, 0);
	if (!writeFile)
	{
		write_log(log_fd, "error:can't open %s for writing,in %d,at %s\n", writepath, __LINE__, __FUNCTION__);
		goto exit;
	}

	fp = fopen(filename.c_str(), "rb");
	if (fp == NULL)
	{
		write_log(log_fd, "error:can't open %s for writing,in %d,at %s\n", filename.c_str(), __LINE__, __FUNCTION__);
		goto exit;
	}

	while ((curSize = fread(buffer, sizeof(char), MAXLENGTH, fp)) > 0)
			hdfsWrite(fs, writeFile, (void *)buffer, curSize);

exit:
	if (fp != NULL)
		fclose(fp);

	hdfsCloseFile(fs, writeFile);
}

void *accept_files(void *args)
{
	int socket_fd = *(int *)args;
	int connect_fd = 0;
	long filesize = 0, ser_recv_size = 0;
	char buffer[MAXLENGTH] = { 0 }, filename[MINILENGTH] = { 0 },
		filemd5[MINILENGTH] = { 0 }, server_recv[MINILENGTH] = { 0 },
		tmpserver[MIDLENGTH] = { 0 }, filesave_path[MIDLENGTH] = { 0 };

	while (1)
	{
		FILE *fp = NULL;
		NOW_TIME now;
		int pos, length;
		bool recv_flag;
		char mk_save_path[MIDLENGTH] = "";

		if ((connect_fd = accept(socket_fd, (struct sockaddr*)NULL, NULL)) == -1)
		{
			write_log(log_fd, "error:accept socket error: %s(errno: %d),in %d,at %s\n", 
				strerror(errno), errno, __LINE__, __FUNCTION__);
			break;
		}

		gettime(&now, false);
		sprintf(tmpserver, "%s/server_%d%02d%02d.ini", log_path, now.year, now.mon, now.day);
		sprintf(filesave_path, "%s/%d%02d%02d/", save_path, now.year, now.mon, now.day);
		sprintf(mk_save_path, "mkdir -p %s", filesave_path);
		exist_file(tmpserver);
		system(mk_save_path);
		
		recv(connect_fd, buffer, MAXLENGTH, 0);
		sscanf(buffer, "%s %s %ld", filename, filemd5, &filesize);

		pthread_mutex_lock(&g_mutex_setflag);
		ser_recv_size = GetPrivateProfileInt(tmpserver, (char *)"default", filename);
		pthread_mutex_unlock(&g_mutex_setflag);
		
		sprintf(server_recv, "%ld", ser_recv_size);
		send(connect_fd, server_recv, sizeof(server_recv), 0);
		if (ser_recv_size == filesize)
			goto exit;

		write_log(log_fd, "filename:%s,filemd5:%s,filesize:%ld\n", filename, filemd5, filesize);
		strcat(filesave_path, filename);
	retry:
		if (access(filesave_path, R_OK) == -1)
			fp = fopen(filesave_path, "wb");
		else
			fp = fopen(filesave_path, "ab");

		if (fp == NULL)
			continue;

		//recv data from client
		bzero(buffer, MAXLENGTH);
		pos = 0, length = 0;
		recv_flag = false;
		while (1)
		{
			if (pos == filesize)
			{
				int flag = strcmp(filemd5, MD5_file(filesave_path, MD5_MODE));
				if (flag == 0)
				{
					send(connect_fd, "true", SOCKET_FLAG, 0);
					break;
				}
				else
				{
					send(connect_fd, "false", SOCKET_FLAG, 0);
					write_log(log_fd, "error:%s send failed, will be send again,in %d,at %s\n", filesave_path, __LINE__, __FUNCTION__);
					goto retry;
				}
			}
			else
			{
				if (recv_flag)//断点续传的点 当长度和flag都不符合，那么说明client断开了,或者就是已经传输完毕了
					break;
			}

			length = recv(connect_fd, buffer, MAXLENGTH, 0);
			pos += length;
			if (length > 0)
			{
				int write_length = fwrite(buffer, sizeof(char), length, fp);
				if (write_length < length)
				{
					write_log(log_fd, "error:%s fwrite failed,in %d,at %s\n", filename, __LINE__, __FUNCTION__);
					break;
				}
			}
			else
				recv_flag = true;

			fflush(fp);
			bzero(buffer, MAXLENGTH);
		}

		pthread_mutex_lock(&g_mutex_setflag);
		SetPrivateProfileInt(tmpserver, (char *)"default", filename, pos);
		pthread_mutex_unlock(&g_mutex_setflag);
		
#ifdef HDFS_FLAG
		pthread_mutex_lock(&g_mutex_qwrite_hdfs);
		qwrite_hdfs.push(string(filesave_path));
		pthread_mutex_unlock(&g_mutex_qwrite_hdfs);
#endif

	exit:
		if (fp != NULL)
			fclose(fp);

		if (connect_fd != 0)
			close(connect_fd);
	}
}

void *write_files_thread(void *agrs)
{
	while (1)
	{
		pthread_mutex_lock(&g_mutex_qwrite_hdfs);
		queue <string> temp;
		if (!qwrite_hdfs.empty())
		{
			swap(temp, qwrite_hdfs);
			pthread_mutex_unlock(&g_mutex_qwrite_hdfs);

			while (!temp.empty())
			{
				write_HDFS(temp.front());
				temp.pop();
			}
		}
		else
			pthread_mutex_unlock(&g_mutex_qwrite_hdfs);

		sleep(10);
	}
}

int main(int argc, char** argv)
{
	int	socket_fd = 0, timeout = 1000, cpu_count = 0;
	struct sockaddr_in servaddr;
	string server_config = "";
	pthread_t queue_id;

	server_config = string(SERVER_CONF);
	cpu_count = GetPrivateProfileInt((char *)server_config.c_str(), (char *)"default", (char *)"cpu_count");
	save_path = GetPrivateProfileString((char *)server_config.c_str(), (char *)"default", (char *)"save_path");
	log_path = GetPrivateProfileString((char *)server_config.c_str(), (char *)"log", (char *)"log_path");
	if ((cpu_count < 0) || (log_path == NULL) || (hdfs_path == NULL))
	{
		write_cmd("error:bad configure file,cpu_count:%d,save_path:%s,log_path:%s,at %d,in %s\n", 
			cpu_count, save_path, log_path, __LINE__, __FUNCTION__);
		goto exit;
	}

	log_fd = log_file(log_path, (char *)"server", MINTUE_MODE);
	if (log_fd == NULL)
		goto exit;

	#ifdef HDFS_FLAG
	hdfs_path = GetPrivateProfileString((char *)server_config.c_str(), (char *)"hdfs", (char *)"hdfs_path");
	hdfs_addr = GetPrivateProfileString((char *)server_config.c_str(), (char *)"hdfs", (char *)"hdfs_addr");
	hdfs_port = GetPrivateProfileInt((char *)server_config.c_str(), (char *)"hdfs", (char *)"hdfs_port");
	if ((hdfs_path == NULL) || (hdfs_port == 0) || (save_path == NULL))
	{
		write_cmd("error:bad configure file,hdfs_path:%s,hdfs_addr:%s,hdfs_port:%d,at %d,in %s\n", 
			hdfs_path, hdfs_addr, hdfs_port, __LINE__, __FUNCTION__);
		goto exit;
	}
	
	fs = hdfsConnect(hdfs_addr, hdfs_port);
	if (!fs)
	{
		write_log(log_fd, "error:failed to connect to hdfs,ip:%s,port:%d,at %d,in %s\n", 
			hdfs_addr, hdfs_port, __LINE__, __FUNCTION__);
		goto exit;
	}
	
	pthread_mutex_init(&g_mutex_qwrite_hdfs, 0);
	#endif

	pthread_mutex_init(&g_mutex_setflag, 0);
	id = (pthread_t *)calloc(20, sizeof(pthread_t));
	if (id == NULL)
	{
		write_log(log_fd, "error:pthread_t id calloc failed,at %d,in:%s\n", __LINE__, __FUNCTION__);
		goto exit;
	}
	
	/* init Socket */
	if( (socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1 )
	{
		write_log(log_fd, "ERROR:create socket error:%s(errno:%d),at %d,in %s\n", 
			strerror(errno), errno, __LINE__, __FUNCTION__);
		goto exit;
	}
	
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(DEFAULT_PORT);

	//bind
	if(bind(socket_fd, (struct sockaddr*)&servaddr, sizeof(servaddr)) == -1)
	{
		write_log(log_fd, "error:bind socket error:%s(errno:%d),at %d,in %s\n", 
			strerror(errno), errno, __LINE__, __FUNCTION__);
		goto exit;
	}

	setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(int));
	
	//listen
	if(listen(socket_fd, 10) == -1)
	{
		write_log(log_fd, "error:listen socket error:%s(errno:%d),at %d,in %s\n", 
			strerror(errno), errno, __LINE__, __FUNCTION__);
		goto exit;
	}

	pthread_create(&queue_id, NULL, write_files_thread, NULL);

	id[0] = pthread_self();
	for (int i = 1; i < cpu_count; i++)
		pthread_create(&id[i], NULL, accept_files, (void *)&socket_fd);

	accept_files((void *)&socket_fd);

exit:
	if (id != NULL)
		free(id);

	if (socket_fd != 0)
		close(socket_fd);

	if (log_fd != NULL)
		close_file(log_fd);

	if (hdfs_addr != NULL)
		hdfs_addr = NULL;

	if (hdfs_path != NULL)
		hdfs_path = NULL;

	if (log_path != NULL)
		log_path = NULL;

	if (save_path != NULL)
		save_path = NULL;

	if (fs != NULL)
		hdfsDisconnect(fs);

	return 0;
}
