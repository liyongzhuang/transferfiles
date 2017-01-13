# Transfer files

#1. Function 
This program will help you to transfer files from some client to one server. Then server will write the files to HDFS of hadoop(if you didn't want,you can use the feature)

#2. Protocol 
The protocol is TCP, without doubt the program support breakpoint resume when some client was abort, and then support MD5 checking. The files will be compress to .gz format by reason of relying on network.

#3. Environment
	First,running environment is Centos6.5. 
	There are some package dependency like zlib, openssl,glib-2.0, jdk1.7.0 version over.
	
	Second,configure HADOOP environment(If you don't need write to hadoop, then skip this sept).
	download the hadoop binary version,here the click:http://hadoop.apache.org/releases.html
	
	Add HADOOP_HOME to environment path, as follows:
	
	# ++++++++++++++ #
	export JAVA_HOME=/usr/java/jdk1.7.0_71
	export HADOOP_HOME=/data/me/hadoop-2.5.2
	export HADOOP_CONF_DIR=/data/me/hadoop-2.5.2/etc/hadoop
	export JAVA_LIBRARY_PATH=/data/me/hadoop-2.5.2/lib/native/Linux-amd64-64
	export PATH=.:$PATH:$JAVA_HOME/bin:$HADOOP_HOME/
	export CLASSPATH=.:/usr/java/jdk1.7.0_71/lib/dt.jar:/usr/java/jdk1.7.0_71/lib/tools.jar:$HADOOP_HOME/etc/hadoop:$HADOOP_HOME/share/hadoop/tools/lib/*:$HADOOP_HOME/share/hadoop/common/lib/*:$HADOOP_HOME/share/hadoop/common/*:$HADOOP_HOME/share/hadoop/hdfs:$HADOOP_HOME/share/hadoop/hdfs/lib/*:$HADOOP_HOME/share/hadoop/hdfs/*:$HADOOP_HOME/share/hadoop/yarn/lib/*:$HADOOP_HOME/share/hadoop/yarn/*:$HADOOP_HOME/share/hadoop/mapreduce/lib/*:$HADOOP_HOME/share/hadoop/mapreduce/*:$HADOOP_HOME/contrib/capacity-scheduler/*.jar:/app/flume-1.5.2/lib/*:$HADOOP_HOME/share/hadoop/tools/lib/commons-lang-2.6.jar::$HADOOP_HOME/share/hadoop/common/hadoop-common-2.5.2.jar:$HADOOP_HOME/share/hadoop/common/lib/commons-logging-1.1.3.jar:$HADOOP_HOME/share/hadoop/common/lib/commons-configuration-1.6.jar:$HADOOP_HOME/share/hadoop/common/lib/guava-11.0.2.jar:$HADOOP_HOME/share/hadoop/common/lib/commons-collections-3.2.1.jar:$HADOOP_HOME/share/hadoop/common/lib/hadoop-auth-2.5.2.jar:$HADOOP_HOME/share/hadoop/common/lib/log4j-1.2.17.jar:$HADOOP_HOME/share/hadoop/common/lib/slf4j-log4j12-1.7.5.jar:$HADOOP_HOME/share/hadoop/common/lib/slf4j-api-1.7.5.jar:$HADOOP_HOME/share/hadoop/hdfs/hadoop-hdfs-2.5.2.jar:$HADOOP_HOME/share/hadoop/common/lib/commons-cli-1.2.jar:$HADOOP_HOME/share/hadoop/common/lib/protobuf-java-2.5.0.jar
	# -------------- # 

#4. Build
	You can enter client and server to build the source code:
		
		make
	
	if you don't need write to hadoop, then use:
		
		make ver=NO_HDFS
		
		
#5. Configure files

	server.conf
		[default]
		cpu_count=10  ##support thread number
		save_path=/data/server   ##server save path
		
		[log]
		log_path=/data4/log/server_logdata  ##server log path

		[hdfs]
		hdfs_path=/user/root/flume ##hadoop hdfs's path
		hdfs_addr=10.0.0.11  ##hdfs addr 
		hdfs_port=9000       ##hdfs port
		
		
	client.conf
		[default]
		source_path=/data/client ##source files path

		[log]
		log_path=/data4/log/client_logdata ##client log path

		[server]
		server_addr=127.0.0.1  ##server ip
		server_port=59001  ##server port 