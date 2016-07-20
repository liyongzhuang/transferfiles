
/* File Name: client.c */
#include <signal.h>
#include "client.h"

pthread_mutex_t g_mutex_sendflag, g_mutex_vector, g_mutex_map;
map <string, string> msetflag;

char *log_path = NULL, *source_path = NULL, *server_addr = NULL, *prefix = NULL, *gzprefix = NULL;
vector <string> send_filename;
string send_path = "";
FILE *log_fd = NULL;
int server_port = 0;

int create_dir(char *dir)
{
	DIR *send_dir = NULL;

	if ((send_dir = opendir(dir)) == NULL)
	{
		int ret = mkdir(dir, S_IRWXU | S_IRWXG | S_IRWXO);
		if (ret != 0)
			return -1;
	}

	if (send_dir != NULL)
		send_dir = NULL;

	return 0;
}

void *gzip_files(void *args)
{
	char send_dir[MIDLENGTH] = { 0 }, source_dir[MIDLENGTH] = { 0 },
		tmp_filename[MIDLENGTH] = { 0 };
	struct dirent *ptr;
	int read_len = 0;
	DIR *dir = NULL;

	while (1)
	{
		NOW_TIME now;

		gettime(&now, false);
		sprintf(source_dir, "%s/%s_%d%02d%02d/", source_path, prefix, now.year, now.mon, now.day);
		sprintf(send_dir, "%s/%s_%d%02d%02d/", send_path.c_str(), gzprefix, now.year, now.mon, now.day);
		sprintf(tmp_filename, "%s/gzclient_%d%02d%02d.ini", log_path, now.year, now.mon, now.day);
		create_dir(send_dir);
		exist_file(tmp_filename);

		dir = opendir(source_dir);
		if (dir == NULL)
		{
			write_log(log_fd, "warning:opendir failed, %s,in %d,at %s\n", source_dir, __LINE__, __FUNCTION__);
			goto exit;
		}

		while ((ptr = readdir(dir)) != NULL)
		{
			char gzfilename[MAXLENGTH] = { 0 }, filename[MAXLENGTH] = { 0 },
				mv_cmd[MAXLENGTH] = { 0 }, buffer[MAXLENGTH] = { 0 };
			FILE *fp = NULL;
			gzFile gzfp = 0;
			int gzflag = 0;			

			gzflag = GetPrivateProfileInt(tmp_filename, (char *)"default", ptr->d_name);
			if ((strcmp(ptr->d_name, ".") == 0) || (strcmp(ptr->d_name, "..") == 0) || 
				(strstr(ptr->d_name, ".gz") != NULL) || (gzflag == 1))
				continue;

			if (strstr(ptr->d_name, "log") != NULL)
			{
				sprintf(filename, "%s%s", source_dir, ptr->d_name);
				sprintf(gzfilename, "%s%s.gz", source_dir, ptr->d_name);
				sprintf(mv_cmd, "mv %s %s", gzfilename, send_dir);
				fp = fopen(filename, "rb");
				gzfp = gzopen(gzfilename, "wb");
				if ((fp == NULL) || (gzfp == 0))
					continue;

				while ((read_len = fread(buffer, sizeof(char), MAXLENGTH, fp)) > 0)
					gzwrite(gzfp, buffer, read_len);

				system(mv_cmd);
				SetPrivateProfileInt(tmp_filename, (char *)"default", ptr->d_name, 1);

				fclose(fp);
				gzclose(gzfp);
			}
		}

	exit:
		sleep(WAITTIME);
	}

	if (dir != NULL)
		dir = NULL;
}

void *send_files(void *args)
{
	FILEINFO finfo = *(FILEINFO *)args;
	write_cmd("finfo:%s\n", finfo.filename);

	int sockfd = 0, npos = 0, timeout = 3000, offset = 0, read_len = 0, filesizes = 0;
	char buffer[MAXLENGTH] = { 0 }, sendline[MAXLENGTH] = { 0 };
	struct sockaddr_in	servaddr;
	FILE *fp = NULL;

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		write_log(log_fd, "error:create socket error: %s(errno: %d),in %d,at %s\n", 
			strerror(errno), errno, __LINE__, __FUNCTION__);
		goto exit;
	}

	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(server_port);
	if (inet_pton(AF_INET, server_addr, &servaddr.sin_addr) <= 0)
	{
		write_log(log_fd, "error:inet_pton error: %s(errno: %d),in %d,at %s\n", 
			strerror(errno), errno, __LINE__, __FUNCTION__);
		goto exit;
	}

	setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(int));

	if (connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0)
	{
		write_log(log_fd, "error:connect error: %s(errno: %d),in %d,at %s\n", 
			strerror(errno), errno, __LINE__, __FUNCTION__);
		goto exit;
	}

	filesizes = get_filesize(finfo.filename);
	sprintf(sendline, "%s %s %ld", basename(finfo.filename), MD5_file(finfo.filename, MD5_MODE), filesizes);
	write_log(log_fd, "filename:%s,filemd5:%s,filesizes:%d\n", basename(finfo.filename), MD5_file(finfo.filename, MD5_MODE), filesizes);
	send(sockfd, sendline, sizeof(sendline), 0);
	recv(sockfd, buffer, MINILENGTH, 0);
	offset = atoi(buffer);
	if (offset == filesizes)
		goto exit;

	pthread_mutex_lock(&g_mutex_map);
	msetflag.insert(make_pair(basename(finfo.filename), finfo.tempfile));
	pthread_mutex_unlock(&g_mutex_map);

retry:
	fp = fopen(finfo.filename, "rb");
	if (fp == NULL)
	{
		write_log(log_fd, "error:fopen %s failed,in %d,at %s!\n", finfo.filename, __LINE__, __FUNCTION__);
		goto exit;
	}

	fseek(fp, offset, SEEK_SET);

	while (1)
	{
		read_len = fread(buffer, sizeof(char), MAXLENGTH, fp);
		if (read_len > 0)
		{
			npos += read_len;
			if (send(sockfd, buffer, read_len, 0) < 0)
				break;
		}
		else
		{
			int sflag = GZ_SENT;
			pthread_mutex_lock(&g_mutex_map);
			map <string, string>::iterator it = msetflag.find(basename(finfo.filename));
			if (it != msetflag.end())
				msetflag.erase(it);
			pthread_mutex_unlock(&g_mutex_map);

			pthread_mutex_lock(&g_mutex_sendflag);
			SetPrivateProfileInt(finfo.tempfile, (char *)"default", basename(finfo.filename), sflag);
			pthread_mutex_unlock(&g_mutex_sendflag);
			
			break;
		}

		bzero(buffer, MAXLENGTH);
	}

	recv(sockfd, buffer, SOCKET_FLAG, 0);
	if (!strcmp(buffer, "false"))
	{
		write_log(log_fd, "error:retry to send data, %s,in %d,at %s\n", finfo.filename, __LINE__, __FUNCTION__);
		goto retry;
	}

exit:
	
	if (fp != NULL)
		fclose(fp);

	if (sockfd != 0)
		close(sockfd);

	if (args != NULL)
		free(args);
}

void free_space()
{
	if (log_fd != 0)
		close_file(log_fd);

	if (prefix != NULL)
		prefix = NULL;

	if (gzprefix != NULL)
		gzprefix = NULL;

	if (log_path != NULL)
		log_path = NULL;

	if (server_addr != NULL)
		server_addr = NULL;

	if (source_path != NULL)
		source_path = NULL;
}

void signalhandler(int signalNo)
{
	map <string, string>::iterator it;
	for (it = msetflag.begin(); it != msetflag.end(); it++)
	{
		pthread_mutex_lock(&g_mutex_sendflag);
		SetPrivateProfileInt((char *)it->second.c_str(), (char *)"default", (char *)it->first.c_str(), GZ_INTERRUPT);
		pthread_mutex_unlock(&g_mutex_sendflag);

		write_log(log_fd, "error:interrupt,the filename:%s,in %d,at %s\n", it->first.c_str(), __LINE__, __FUNCTION__);
	}

	free_space();

	exit(0);
}

int main(int argc, char** argv)
{
	pthread_t gzip_id;
	string client_config = "";
	
	client_config = string(CONFIG_PATH) + string(CLIENT_CONF);
	log_path = GetPrivateProfileString((char *)client_config.c_str(), (char *)"log", (char *)"log_path");
	source_path = GetPrivateProfileString((char *)client_config.c_str(), (char *)"default", (char *)"source_path");//not me created.
	prefix = GetPrivateProfileString((char *)client_config.c_str(), (char *)"default", (char *)"prefix");
	gzprefix = GetPrivateProfileString((char *)client_config.c_str(), (char *)"default", (char *)"gzprefix");
	server_addr = GetPrivateProfileString((char *)client_config.c_str(), (char *)"server", (char *)"server_addr");
	server_port = GetPrivateProfileInt((char *)client_config.c_str(), (char *)"server", (char *)"server_port");
	if ((log_path == NULL) || (source_path == NULL) || (server_addr == NULL) || (server_port == 0))
	{
		write_cmd("error:bad configure file,send_path:%s,log_path:%s,source:%s,server_addr:%s,server_port:%d,in %d,at %s\n", 
			send_path.c_str(), log_path, source_path, server_addr, server_port, __LINE__, __FUNCTION__);
		goto exit;
	}

	// create log system
	log_fd = log_file(log_path, (char *)"client", MINTUE_MODE);
	if (log_fd == NULL)
	{
		write_cmd("can't create log_fd, in %d, at %s\n", __LINE__, __FUNCTION__);
		goto exit;
	}
		
	/* init mutex and signal */
	signal(SIGINT, signalhandler);
	signal(SIGTERM, signalhandler);
	signal(SIGKILL, signalhandler);
	pthread_mutex_init(&g_mutex_sendflag, 0);
	pthread_mutex_init(&g_mutex_vector, 0);
	pthread_mutex_init(&g_mutex_map, 0);
	send_path = string(source_path);
	pthread_create(&gzip_id, NULL, gzip_files, NULL);

	while (1)
	{
		sleep(WAITTIME);

		char send_dir[MIDLENGTH] = { 0 }, tmp_dir[MIDLENGTH] = { 0 };
		struct dirent *ptr;
		NOW_TIME now;
		DIR *dir = NULL;
		int i = 0, vsize = 0;
		
		gettime(&now, false);
		sprintf(send_dir, "%s/%s_%d%02d%02d", send_path.c_str(), gzprefix, now.year, now.mon, now.day);
		sprintf(tmp_dir, "%s/client_%d%02d%02d.ini", log_path, now.year, now.mon, now.day);
		exist_file(tmp_dir);

		dir = opendir(send_dir);
		if (dir == NULL)
		{
			write_log(log_fd, "warning:opendir failed, send_dir:%s,in %d,at %s\n", send_dir, __LINE__, __FUNCTION__);
			continue;
		}

		while ((ptr = readdir(dir)) != NULL)
		{
			int send_flag = 0;
			
			if (strcmp(ptr->d_name, ".") == 0 || strcmp(ptr->d_name, "..") == 0)
				continue;

			pthread_mutex_lock(&g_mutex_sendflag);
			send_flag = GetPrivateProfileInt(tmp_dir, (char *)"default", ptr->d_name);
			pthread_mutex_unlock(&g_mutex_sendflag);

			if ((strstr(ptr->d_name, ".gz") != NULL) && ((send_flag == GZ_UNSENT) || (send_flag == GZ_INTERRUPT)))
			{
				pthread_mutex_lock(&g_mutex_vector);
				send_filename.push_back(string(ptr->d_name));
				pthread_mutex_unlock(&g_mutex_vector);
			}
		}

		pthread_mutex_lock(&g_mutex_vector);
		vsize = send_filename.size();
		if (vsize > 0)
		{
			vector <string> temp;

			temp.swap(send_filename);
			pthread_mutex_unlock(&g_mutex_vector);

			pthread_t id[vsize];
			for (i = 0; i < vsize; i++)
			{
				char filepath[MAXLENGTH] = { 0 };
				
				FILEINFO *file_info;
				file_info = (FILEINFO *)malloc(sizeof(FILEINFO));

				sprintf(filepath, "%s/%s", send_dir, temp[i].c_str());
				strcpy(file_info->filename, filepath);
				strcpy(file_info->tempfile, tmp_dir);

				pthread_create(id + 1, NULL, send_files, (void *)file_info);

				pthread_mutex_lock(&g_mutex_sendflag);
				SetPrivateProfileInt(tmp_dir, (char *)"default", (char *)temp[i].c_str(), GZ_SENDING);
				pthread_mutex_unlock(&g_mutex_sendflag);
			}
		}
		else
			pthread_mutex_unlock(&g_mutex_vector);

	}

exit:
	free_space();

	return 0;
}
